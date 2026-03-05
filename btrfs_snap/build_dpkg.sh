#!/bin/bash

set -eu
(set -o pipefail 2>/dev/null) && set -o pipefail || true

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "${SCRIPT_DIR}"

chmod -R u=rwX,go=rX .
chmod 755 usr/sbin/btrfssnap_common
chmod 755 usr/sbin/btrfssnap_root
chmod 755 usr/sbin/btrfssnap_list
chmod 755 usr/sbin/btrfssnap_mount
chmod 755 usr/sbin/btrfssnap_unmount
chmod 755 usr/sbin/btrfssnap_retention
chmod 755 usr/sbin/btrfssnap_snapshot

dpkg-buildpackage -us -uc

deb_file=""
for candidate in ../openmediavault-btrfs-snap_*_all.deb; do
    if [ -f "${candidate}" ]; then
        deb_file="${candidate}"
    fi
done
[ -n "${deb_file}" ] || {
    echo "Built .deb file not found." >&2
    exit 1
}

dpkg -r openmediavault-btrfs-snap || true
dpkg -i "${deb_file}"
omv-mkworkbench all
systemctl restart openmediavault-engined
