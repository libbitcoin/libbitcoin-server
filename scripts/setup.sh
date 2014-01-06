#!/bin/bash
#
# Designed for a fairly Debian standard. 
#
# Compatible with Fedora standard too.
# 
# Debian | Fedora 
#
# Script to setup obworker an obbalancer daemons.
#
# Requires previous instalation of libbitcoin, libwallet and obelisk.
# Download and execute the install-sx.sh:
# <wget http://sx.dyne.org/install-sx.sh>
# And run:
# <sudo bash install-sx.sh>
# If you are setting up a oblisk node you need to run this script to create and configure
# obworker and obbalancer daemons.
#
# 
set -e
if [ `id -u` = "0" ]; then
    SOURCE=/usr/local/src/obelisk-git/scripts
    WORKER=/etc/obelisk/worker.cfg
else    
    echo 
    echo " --> ERROR: You need be root to run this script."
    echo "     <sudo bash $SOURCE/setup.sh>"
fi

create_ob_user(){
    if [ -z "`getent passwd ob`" ]; then 
    	echo
        echo " --> Creating ob user"
    	echo
    	adduser --system --disabled-password --shell /bin/bash --home /var/lib/blockchain --group ob
    	sx initchain /var/lib/blockchain/
    else
	    sx initchain /var/lib/blockchain/
	    echo " --> ob user already exists: skipping..."
        echo
    fi
}

config_logfiles(){
    if [ -n "`grep -E \"\/var\/lib\/blockchain\/\" /etc/obelisk/worker.cfg`" ]; then
    	sed -r -i -e "s/blockchain\-path =.*?$/blockchain-path = \"\/var\/lib\/blockchain\/\"/g" $WORKER 
    	sed -r -i -e "s/debug\.log/\/var\/lib\/blockchain\/debug.log/g" $WORKER 
    	sed -r -i -e "s/error\.log/\/var\/lib\/blockchain\/error.log/g" $WORKER
    else
	    echo " ERROR: --> You need to have installed libbitcoin, sx and obelisk before run this script."
	    echo " Read the header of this script:"
	    echo " <cat /user/local/src/obelist-git/scripts/setup.sh>"
        echo
    fi
}

config_logrotate(){
    ln -sf $SOURCE/logrotate.sh /etc/logrotate.d/logrotate.sh
}

up_limits(){
    bash -c "echo \"ob  soft  nofile 4096\" >> /etc/security/limits.conf"
    bash -c "sudo echo \"ob  hard  nofile 65000\" >> /etc/security/limits.conf"
    bash -c "sudo echo \"session required pam_limits.so\" >> /etc/pam.d/common-session"
}

setup_init_scripts(){
    echo " --> Setting up init scripts..."
    echo
    chmod +x $SOURCE/init.d/obworker      
    chmod +x $SOURCE/init.d/obbalancer      
    ln -sf $SOURCE/init.d/obworker /etc/init.d/obworker     
    ln -sf $SOURCE/init.d/obbalancer /etc/init.d/obbalancer
    update-rc.d obworker defaults 80
    echo
    update-rc.d obbalancer defaults 81
    echo
    echo "Starting services..."
    echo
    service obworker start
    echo
    service obbalancer start
    echo "All done!"
    echo
}

create_ob_user
config_logfiles
config_logrotate
up_limits
setup_init_scripts

