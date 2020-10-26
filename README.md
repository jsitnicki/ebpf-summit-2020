# Steering connections to sockets with BPF socket lookup hook

Code and instructions for the lighting talk at [eBPF Summit 2020](https://ebpf.io/summit-2020/).

## Goal

Set up an echo service on 3 ports, but using just one TCP listening socket.

We will use BPF socket lookup to dispatch connection to the echo server.

## Prepare the VM

Bring up the VM:

```
host $ vagrant up
…
```

Build BPF tooling and the BPF dispatch program on the VM:

```
host $ vagrant ssh
vm $ uname -a
Linux fedora32 5.9.1-36.vanilla.1.fc32.x86_64 #1 SMP Sat Oct 17 07:53:17 UTC 2020 x86_64 x86_64 x86_64 GNU/Linux
vm $ cd /vagrant/
vm $ make
…
```

## Start the echo server and test it

```
vm $ nc -4kle /bin/cat 127.0.0.1 7777 &
[1] 24533
$ ss -4tlpn sport = 7777
State    Recv-Q   Send-Q     Local Address:Port     Peer Address:Port  Process
LISTEN   0        10             127.0.0.1:7777          0.0.0.0:*      users:(("nc",pid=24533,fd=3))
vm $ { echo test; sleep 0.1; } | nc -4 127.0.0.1 7777
test
```

## Find VM IP and check open ports

```
vm $ ip -4 addr show eth0
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP group default qlen 1000
    altname enp0s5
    altname ens5
    inet 192.168.122.247/24 brd 192.168.122.255 scope global dynamic noprefixroute eth0
       valid_lft 2834sec preferred_lft 2834sec
```

VM external IP is `192.168.122.247`.

```
host $ nmap -sT -p 1-1000 192.168.122.247
Starting Nmap 7.80 ( https://nmap.org ) at 2020-10-28 17:10 CET
Nmap scan report for 192.168.122.247
Host is up (0.00021s latency).
Not shown: 999 closed ports
PORT   STATE SERVICE
22/tcp open  ssh

Nmap done: 1 IP address (1 host up) scanned in 0.08 seconds
```

Only port `22` is open.

## Load `echo_dispatch` BPF program

```
vm $ cd /vagrant
vm $ sudo ./bpftool prog load ./echo_dispatch.bpf.o /sys/fs/bpf/echo_dispatch_prog
vm $ sudo ./bpftool prog show pinned /sys/fs/bpf/echo_dispatch_prog
49: sk_lookup  name echo_dispatch  tag da043673afd29081  gpl
        loaded_at 2020-10-28T16:13:42+0000  uid 0
        xlated 272B  jited 164B  memlock 4096B  map_ids 3,4
        btf_id 4
```

## Pin BPF maps used by `echo_dispatch`

Mount a dedicated bpf file-system for our user `vagrant`:

```
vm $ mkdir ~/bpffs
vm $ sudo mount -t bpf none ~/bpffs
vm $ sudo chown vagrant.vagrant ~/bpffs
```

Pin `echo_ports` map

```
vm $ sudo ./bpftool map show name echo_ports
3: hash  name echo_ports  flags 0x0
        key 2B  value 1B  max_entries 1024  memlock 86016B
vm $ sudo ./bpftool map pin name echo_ports ~/bpffs/echo_ports
```

Pin `echo_socket` map

```
vm $ sudo ./bpftool map show name echo_socket
4: sockmap  name echo_socket  flags 0x0
        key 4B  value 8B  max_entries 1  memlock 4096B
vm $ sudo ./bpftool map pin name echo_socket ~/bpffs/echo_socket
```

Give user `vagrant` access to pinned maps:

```
vm $ sudo chown vagrant.vagrant ~/bpffs/{echo_ports,echo_socket}
```

## Insert Ncat socket into `echo_socket` map

Find socket owner PID and FD number:

```
vm $ ss -tlpne 'sport = 7777'
State    Recv-Q   Send-Q     Local Address:Port     Peer Address:Port  Process
LISTEN   0        10             127.0.0.1:7777          0.0.0.0:*      users:(("nc",pid=24533,fd=3)) uid:1000 ino:38462 sk:1 <->
```

Put the socket into `echo_socket` map using `socket-update` tool:

```
vm $ ./sockmap-update 24533 3 ~/bpffs/echo_socket
vm $ ./bpftool map dump pinned ~/bpffs/echo_socket
$ ./bpftool map dump pinned ~/bpffs/echo_socket
key: 00 00 00 00  value: 01 00 00 00 00 00 00 00
Found 1 element
```

Notice the value under key `0x00` is the socket cookie (`0x01`) we saw in `ss`
output (`sk:1`). Socket cookie is a unique identifier for a socket description
inside the kernel.

## Attach `echo_dispatch` to network namespace

Create a BPF link between the current network namespace and the loaded
`echo_dispatch` program with the `sk-lookup-attach` tool:

```
vm $ sudo ./sk-lookup-attach /sys/fs/bpf/echo_dispatch_prog /sys/fs/bpf/echo_dispatch_link
```

Examine the created BPF link:

```
vm $ sudo ./bpftool link show pinned /sys/fs/bpf/echo_dispatch_link
4: netns  prog 49
        netns_ino 4026531992  attach_type sk_lookup
vm $ ls -l /proc/self/ns/net
lrwxrwxrwx. 1 vagrant vagrant 0 Oct 28 16:29 /proc/self/ns/net -> 'net:[4026531992]'
```

Notice the BPF link can be matched with the network namespace via its inode number.

## Enable echo service on ports 7, 77, 777

Populate `echo_ports` map with entries for open ports `7` (`0x7`), `77`
(`0x4d`), and `7777` (`0x0309`):

```
vm $ ./bpftool map update pinned ~/bpffs/echo_ports key 0x07 0x00 value 0x00
vm $ ./bpftool map update pinned ~/bpffs/echo_ports key 0x4d 0x00 value 0x00
vm $ ./bpftool map update pinned ~/bpffs/echo_ports key 0x09 0x03 value 0x00
vm $ ./bpftool map dump pinned ~/bpffs/echo_ports
key: 4d 00  value: 00
key: 07 00  value: 00
key: 09 03  value: 00
Found 3 elements
```

## Re-scan open ports on VM

```
host $ nmap -sT -p 1-1000 192.168.122.247
Starting Nmap 7.80 ( https://nmap.org ) at 2020-10-28 17:34 CET
Nmap scan report for 192.168.122.247
Host is up (0.00017s latency).
Not shown: 996 closed ports
PORT    STATE SERVICE
7/tcp   open  echo
22/tcp  open  ssh
77/tcp  open  priv-rje
777/tcp open  multiling-http

Nmap done: 1 IP address (1 host up) scanned in 0.09 seconds
```

Notice echo ports we have just configured are open.

## Test the echo service on all open ports

```
host $ { echo 'Hip'; sleep 0.1; }     | nc -4 192.168.122.247   7 && \
       { echo 'hip'; sleep 0.1; }     | nc -4 192.168.122.247  77 && \
       { echo 'hooray!'; sleep 0.1; } | nc -4 192.168.122.247 777
Hip
hip
hooray!
```
