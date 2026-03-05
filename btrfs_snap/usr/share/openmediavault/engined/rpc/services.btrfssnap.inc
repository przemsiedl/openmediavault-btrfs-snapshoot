<?php
namespace Engined\Rpc;
use OMV\Exception;

class BtrfsSnap extends \OMV\Rpc\ServiceAbstract
{
    private const DEFAULT_UI_PATH = "srv";
    private const CMD_ROOT = "/usr/sbin/btrfssnap_root";
    private const CMD_LIST = "/usr/sbin/btrfssnap_list";
    private const CMD_MOUNT = "/usr/sbin/btrfssnap_mount";
    private const CMD_UNMOUNT = "/usr/sbin/btrfssnap_unmount";
    private const CMD_RETENTION = "/usr/sbin/btrfssnap_retention";
    private const CMD_SNAPSHOT = "/usr/sbin/btrfssnap_snapshot";
    private const DEFAULT_RETENTION_LIMIT = 100;

    private function normalizeRootRelativePath($path)
    {
        $relativePath = trim((string) $path);
        if (strpos($relativePath, "..") !== false) {
            throw new Exception("Invalid path.");
        }
        $relativePath = trim(str_replace("\\", "/", $relativePath), "/");
        if ($relativePath === "") {
            throw new Exception("Path '/' is not allowed.");
        }
        return $relativePath;
    }

    private function normalizeSnapshotRelativePath($path)
    {
        $path = trim((string) $path);
        if ($path === "") {
            throw new Exception("Parameter 'snapshot_path' is required.");
        }
        if (strpos($path, "..") !== false) {
            throw new Exception("Invalid snapshot path.");
        }
        return ltrim($path, "/");
    }

    private function ensureManagedSnapshotPath($path)
    {
        $normalized = $this->normalizeSnapshotRelativePath($path);
        $name = basename($normalized);
        if (!preg_match('/^v\d{8}-\d{4}$/', $name)) {
            throw new Exception("Snapshot is not managed by this plugin: " . $normalized);
        }
        return $normalized;
    }

    private function normalizeRetentionLimit($value)
    {
        $limit = trim((string) $value);
        if ($limit === "" || !preg_match('/^\d+$/', $limit)) {
            throw new Exception("Parameter 'retention_limit' must be a positive integer.");
        }
        $numeric = (int) $limit;
        if ($numeric < 1) {
            throw new Exception("Parameter 'retention_limit' must be greater than 0.");
        }
        return $numeric;
    }

    private function getRootMountpointUuid()
    {
        $db = \OMV\Config\Database::getInstance();
        $objects = $db->getByFilter("conf.system.filesystem.mountpoint", [
            "operator" => "stringEquals",
            "arg0" => "dir",
            "arg1" => "/"
        ]);
        if (empty($objects)) {
            return "";
        }
        return (string) $objects[0]->get("uuid");
    }

    private function buildCommand(array $parts)
    {
        $escaped = array_map(function($part) {
            return escapeshellarg((string) $part);
        }, $parts);
        return implode(" ", $escaped) . " 2>&1";
    }

    private function executeCommand(array $parts, $allowFailure = false)
    {
        $cmd = $this->buildCommand($parts);
        $output = [];
        $rc = 0;
        exec($cmd, $output, $rc);
        if ($rc !== 0 && !$allowFailure) {
            $msg = trim(implode("\n", $output));
            if ($msg === "") {
                $msg = "Command failed: " . $parts[0];
            }
            throw new Exception($msg);
        }
        return [
            "rc" => $rc,
            "output" => $output
        ];
    }

    private function getConfiguredBtrfsRoot()
    {
        $result = $this->executeCommand([self::CMD_ROOT, "get"], true);
        if ($result["rc"] !== 0 || empty($result["output"])) {
            return "";
        }
        return trim((string) $result["output"][0]);
    }

    private function getConfiguredRetentionLimit()
    {
        $result = $this->executeCommand([self::CMD_RETENTION, "get"], true);
        if ($result["rc"] !== 0 || empty($result["output"])) {
            return self::DEFAULT_RETENTION_LIMIT;
        }
        return $this->normalizeRetentionLimit($result["output"][0]);
    }

    public function getName()
    {
        return "BtrfsSnap";
    }

    public function initialize()
    {
        $this->registerMethod("getSnapshotList");
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod("createSnapshot");
        $this->registerMethod("mountSnapshot");
        $this->registerMethod("unmountSnapshot");
    }

    public function getSnapshotList($params, $context)
    {
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        $result = $this->executeCommand([self::CMD_LIST]);
        $rootMntentUuid = $this->getRootMountpointUuid();
        $snaps = [];

        foreach ($result["output"] as $line) {
            $line = rtrim((string) $line, "\r\n");
            if ($line === "") {
                continue;
            }
            $parts = explode("\t", $line, 4);
            if (count($parts) < 3) {
                continue;
            }
            $mountDir = count($parts) >= 4 ? (string) $parts[3] : "";
            $snaps[] = [
                "name" => (string) $parts[0],
                "path" => (string) $parts[1],
                "is_mounted" => ((string) $parts[2]) === "1",
                "mount_dir" => $mountDir,
                "root_mntent_uuid" => $rootMntentUuid
            ];
        }

        return $snaps;
    }

    public function getSettings($params, $context)
    {
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        $configuredRoot = $this->getConfiguredBtrfsRoot();
        $path = self::DEFAULT_UI_PATH;
        if ($configuredRoot !== "") {
            $path = ltrim($configuredRoot, "/");
        }

        $rootMntentUuid = $this->getRootMountpointUuid();
        if ($rootMntentUuid === "") {
            throw new Exception(
                "Folder picker from '/' requires a mountpoint entry for '/' in OMV (conf.system.filesystem.mountpoint)."
            );
        }

        return [
            "path" => $path,
            "configured_btrfs_root" => $configuredRoot,
            "retention_limit" => $this->getConfiguredRetentionLimit(),
            "root_mntent_uuid" => $rootMntentUuid
        ];
    }

    public function setSettings($params, $context)
    {
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        if (!isset($params["path"])) {
            throw new Exception("Parameter 'path' is required.");
        }
        if (!isset($params["root_mntent_uuid"])) {
            throw new Exception("Parameter 'root_mntent_uuid' is required.");
        }
        if (!isset($params["retention_limit"])) {
            throw new Exception("Parameter 'retention_limit' is required.");
        }

        $rootMntentUuid = trim((string) $params["root_mntent_uuid"]);
        if ($rootMntentUuid !== $this->getRootMountpointUuid()) {
            throw new Exception("Invalid root mountpoint UUID.");
        }

        $relativePath = $this->normalizeRootRelativePath($params["path"]);
        $btrfsRoot = "/" . $relativePath;
        $retentionLimit = $this->normalizeRetentionLimit($params["retention_limit"]);

        $this->executeCommand([self::CMD_RETENTION, "set", (string) $retentionLimit]);
        $result = $this->executeCommand([self::CMD_ROOT, "set", $btrfsRoot]);
        $configured = empty($result["output"]) ? $btrfsRoot : trim((string) $result["output"][0]);

        return [
            "path" => $configured,
            "configured_btrfs_root" => $configured,
            "retention_limit" => $retentionLimit
        ];
    }

    public function createSnapshot($params, $context)
    {
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        $result = $this->executeCommand([self::CMD_SNAPSHOT]);
        $snapshotName = "";
        if (!empty($result["output"])) {
            $snapshotName = trim((string) $result["output"][0]);
        }

        return [
            "snapshot_name" => $snapshotName
        ];
    }

    public function mountSnapshot($params, $context)
    {
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        if (!isset($params["snapshot_path"])) {
            throw new Exception("Parameter 'snapshot_path' is required.");
        }
        if (!isset($params["mount_path"])) {
            throw new Exception("Parameter 'mount_path' is required.");
        }
        if (!isset($params["root_mntent_uuid"])) {
            throw new Exception("Parameter 'root_mntent_uuid' is required.");
        }

        $rootMntentUuid = trim((string) $params["root_mntent_uuid"]);
        if ($rootMntentUuid !== $this->getRootMountpointUuid()) {
            throw new Exception("Invalid root mountpoint UUID.");
        }

        $snapshotPath = $this->ensureManagedSnapshotPath($params["snapshot_path"]);
        $mountRelativePath = $this->normalizeRootRelativePath($params["mount_path"]);
        $mountDir = "/" . $mountRelativePath;
        $result = $this->executeCommand([self::CMD_MOUNT, $snapshotPath, $mountDir]);
        $mounted = empty($result["output"]) ? $mountDir : trim((string) $result["output"][0]);

        return [
            "mount_dir" => $mounted
        ];
    }

    public function unmountSnapshot($params, $context)
    {
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        if (!isset($params["snapshot_path"])) {
            throw new Exception("Parameter 'snapshot_path' is required.");
        }

        $snapshotPath = $this->ensureManagedSnapshotPath($params["snapshot_path"]);
        $result = $this->executeCommand([self::CMD_UNMOUNT, $snapshotPath]);
        $unmounted = 0;
        if (!empty($result["output"])) {
            $unmounted = (int) trim((string) $result["output"][0]);
        }

        return [
            "unmounted" => $unmounted
        ];
    }
}
