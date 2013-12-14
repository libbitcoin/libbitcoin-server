#!/bin/bash

printPlain() { /bin/echo -e "${1}" >&2; }
printGreen() { /bin/echo -e "\033[32;1m${1}\033[0m" >&2; }
printBlue() { /bin/echo -e "\033[34;1m${1}\033[0m" >&2; }
printYellow() { /bin/echo -e "\033[33;1m${1}\033[0m" >&2; }
printRed() { /bin/echo -e "\033[31;1m${1}\033[0m" >&2; }

printGreen "Installing Required Packages"
sudo apt-get -y update
sudo apt-get -y install wget git-core

#install and configure sx
INSTALL_SX=1
if [ -e /usr/local/bin/sx ]; then
  read -p "sx is already installed, reinstall? (y/n): " -e REINSTALL_SX
  if [ "${REINSTALL_SX}" != 'y' ]; then
    INSTALL_SX=0
  fi
fi
if [ $INSTALL_SX -eq 1 ]; then
 printGreen "Installing sx..."
 CURDIR=`pwd`
 cd /tmp
 wget http://sx.dyne.org/install-sx.sh
 sudo sed -r -i -e "s/apt-get install/apt-get -y install/g" /tmp/install-sx.sh
 #^ makes the script run a bit more user friendly
 sudo bash ./install-sx.sh
 cd ${CURDIR}
fi

#create ob user
if [ -z "`getent passwd ob`" ]; then
  printGreen "Creating ob user"
  sudo adduser --system --disabled-password --shell /bin/bash --home /var/lib/blockchain --group ob
else
  printYellow "ob user already exists: skipping..."
fi

#setup blockchain data dir 
if [ ! -e /var/lib/blockchain/block ]; then
 sudo sx initchain /var/lib/blockchain/
fi
sudo chown -R ob:ob /var/lib/blockchain

#configure obworker to throw its logfiles into /var/lib/blockchain/
if [ -z "`grep -E \"\/var\/lib\/blockchain\/\" /etc/obelisk/worker.cfg`" ]; then
 sudo sed -r -i -e "s/blockchain\-path =.*?$/blockchain-path = \"\/var\/lib\/blockchain\/\"/g" /etc/obelisk/worker.cfg 
 sudo sed -r -i -e "s/debug\.log/\/var\/lib\/blockchain\/debug.log/g" /etc/obelisk/worker.cfg 
 sudo sed -r -i -e "s/error\.log/\/var\/lib\/blockchain\/error.log/g" /etc/obelisk/worker.cfg
fi

#to prevent the error.log and debug.log files from getting too big with obworker...
sudo ln -sf /opt/mastercoind/sysinstall/linux/logrotate.d/obworker /etc/logrotate.d/obworker

#up open file limits for ob user (which obelisk runs as) because libbitcoin needs higher limits
sudo bash -c "echo \"ob  soft  nofile 4096\" >> /etc/security/limits.conf"
sudo bash -c "sudo echo \"ob  hard  nofile 65000\" >> /etc/security/limits.conf"
sudo bash -c "sudo echo \"session required pam_limits.so\" >> /etc/pam.d/common-session"

#setup init scripts
printBlue "Setting up init scripts..."
sudo ln -sf /opt/mastercoind/sysinstall/linux/init.d/obworker /etc/init.d/obworker
sudo ln -sf /opt/mastercoind/sysinstall/linux/init.d/obbalancer /etc/init.d/obbalancer
sudo update-rc.d obworker defaults 80
sudo update-rc.d obbalancer defaults 81

printBlue "Starting services..."
sudo service obworker start

#create necessary local config files for sx
cp /usr/local/share/sx/sx.cfg ~/.sx.cfg
sudo cp /usr/local/share/sx/sx.cfg ~mastercoind/.sx.cfg

printGreen "All done!"

