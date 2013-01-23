CC = $(CROSS_COMPILE)gcc
CFLAGS = -O2 -Wall
CFLAGS += -I/usr/include -I/include

wlan_prov: wlan_provisioning.c
	$(CC) -o wlan_prov wlan_provisioning.c

install:
	@mkdir -p /usr/sbin/
	@echo Copy files to /usr/sbin/
	@cp -f ./wlan_prov /usr/sbin/
	@chmod 755 /usr/sbin/wlan_prov

clean:
	@rm -f wlan_prov
