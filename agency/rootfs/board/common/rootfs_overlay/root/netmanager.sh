 
#!/bin/sh

#
# This script configure the network allowin ME to access the network thru a NAT
#

# Configure NAT
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

# Enable ip forwarding
sysctl -w net.ipv4.ip_forward=1


# Configuring each domain
for i in {0..4}
do
    # Enable routing 
    iptables -A FORWARD -i eth$i -o vif0 -m state --state RELATED
    iptables -A FORWARD -i vif$i -o eth0 -j ACCEPT
    
    # disable ipv6
    sysctl -w net.ipv6.conf.vif$i.disable_ipv6=1
    
    # Disable Networkmanager (we don't want dhcp)
    nmcli dev set vif$i managed no
done

