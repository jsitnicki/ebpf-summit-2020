# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "fedora/32-cloud-base"
  config.vm.box_version = "32.20200422.0"

  config.vm.hostname = "bpf-sk-lookup"

  config.vm.provider :libvirt do |libvirt|
    libvirt.memory = 8192
    libvirt.cpus = 4
  end

  config.vm.provision "shell", inline: <<-SHELL
    # Configure dnf repo for vanilla kernel
    curl -s https://repos.fedorapeople.org/repos/thl/kernel-vanilla.repo | \
      sudo tee /etc/yum.repos.d/kernel-vanilla.repo
    dnf config-manager --set-enabled kernel-vanilla-stable
    # Install latest vanilla kernel
    dnf update -y kernel-core
    # Install development tools
    dnf install -y clang gcc make elfutils-libelf-devel llvm nmap-ncat
    # Reboot into new kernel
    reboot
  SHELL
end
