 
#!/bin/sh

#
# This script configure the network allowin ME to access the network thru a NAT
#

USE_NAT=true
USE_BRIDGE=true

BRIDGE_NAME=soo-br0
OUT_IF_NAME=eth0

ME_NET_IP=10.204.0.1
ME_NET_MASK=255.255.0.0


# Configure NAT
if [ "$USE_BRIDGE" = true ]; then
    iptables -t nat -A POSTROUTING -o $OUT_IF_NAME -j MASQUERADE
fi
    
# Enable ip forwarding
sysctl -w net.ipv4.ip_forward=1


# Forward the bridge to the internet thru nat
if [ "$USE_NAT" = true ] && [ "$USE_BRIDGE" = true ]; then
    iptables -A FORWARD -i $OUT_IF_NAME -o vif$i -m state --state RELATED
    iptables -A FORWARD -i vif$i -o $OUT_IF_NAME -j ACCEPT
fi


if [ "$USE_BRIDGE" = true ]; then
    brctl addbr $BRIDGE_NAME
    ifconfig $BRIDGE_NAME $ME_NET_IP netmask $ME_NET_MASK
fi


for i in {0..4}
do
    # disable ipv6
    sysctl -w net.ipv6.conf.vif$i.disable_ipv6=1
    
    # Disable Networkmanager (we don't want dhcp)
    nmcli dev set vif$i managed no
    
    
    if [ "$USE_BRIDGE" = true ]; then
        #add to bridge
        brctl addif $BRIDGE_NAME vif$i
    else
        # Enable routing for each virtual interfaces
        iptables -A FORWARD -i $OUT_IF_NAME -o vif$i -m state --state RELATED
        iptables -A FORWARD -i vif$i -o $OUT_IF_NAME -j ACCEPT
    fi
    
    ifdown vif$i
done


# Configuring each domain
#for i in {0..4}
#do
    
#    iptables -A FORWARD -i eth$i -o vif0 -m state --state RELATED
#    iptables -A FORWARD -i vif$i -o eth0 -j ACCEPT
    
    # disable ipv6
#    sysctl -w net.ipv6.conf.vif$i.disable_ipv6=1
    
    # Disable Networkmanager (we don't want dhcp)
#    nmcli dev set vif$i managed no
#done

