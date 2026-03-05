# openmediavault-btrfs-snap

An OpenMediaVault plugin for managing BTRFS snapshots:
- listing snapshots,
- mounting and unmounting snapshots,
- creating snapshots with retention,
- configuring BTRFS root and retention from the OMV UI.

## Requirements

- OpenMediaVault 6+,
- a BTRFS filesystem,
- root privileges for mount/umount/snapshot operations.

## Build and Install

Run from the repository directory:

```bash
./build_dpkg.sh
```

The script will:
- build the Debian package,
- reinstall `openmediavault-btrfs-snap`,
- refresh the workbench (`omv-mkworkbench all`),
- restart `openmediavault-engined`.

The script works regardless of where the repository is located (it uses its own directory).

## Configuration

Configuration file:

`/etc/openmediavault/btrfssnap.conf`

Supported keys:
- `BTRFS_ROOT` - BTRFS root path used for listing and snapshot operations,
- `RETENTION_LIMIT` - maximum number of auto-created snapshots to keep.

Default `RETENTION_LIMIT`: `100`.

## CLI Commands

All commands are installed in `usr/sbin`:

- `btrfssnap_root get|set <absolute_path>`
- `btrfssnap_retention get|set <positive_integer>`
- `btrfssnap_list`
- `btrfssnap_mount <snapshot_path> <absolute_mount_dir>`
- `btrfssnap_unmount <snapshot_path>`
- `btrfssnap_snapshot`

## Managed Snapshot Name Pattern

The plugin manages only snapshots with names matching:

`vYYYYMMDD-HHMM`

Example:
- `v20251003-0500`

Snapshots that do not match this pattern are ignored by the plugin UI.

## Snapshot Creation and Retention

Command:

```bash
/usr/sbin/btrfssnap_snapshoot
```

Behavior:
- temporarily mounts BTRFS top-level (`subvolid=5`),
- creates a read-only snapshot from `base` to `vYYYYMMDD-HHMM`,
- deletes oldest snapshots above `RETENTION_LIMIT`,
- cleans up the temporary mount.

## Scheduling in OMV

To run snapshots automatically:

1. Go to `System -> Scheduled Tasks -> Add -> Command`
2. Command: `/usr/sbin/btrfssnap_snapshoot`
3. User: `root`
4. Choose a schedule and save.

Example cron schedule (daily at 05:00):

```cron
0 5 * * * /usr/sbin/btrfssnap_snapshoot
```
