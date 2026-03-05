chmod -R 644 .
chmod 755 usr/sbin/btrfssnap_common
chmod 755 usr/sbin/btrfssnap_root
chmod 755 usr/sbin/btrfssnap_list
chmod 755 usr/sbin/btrfssnap_mount
chmod 755 usr/sbin/btrfssnap_unmount
chmod 755 usr/sbin/btrfssnap_retention
chmod 755 usr/sbin/btrfssnap_snapshoot
dpkg-buildpackage -us -uc
dpkg -r openmediavault-btrfs-snap
dpkg -i ../openmediavault-btrfs-snap_1.0-1_all.deb
omv-mkworkbench all
systemctl restart openmediavault-engined
chmod -R 777 .

