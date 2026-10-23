<?php
/**
 * This file is part of openmediavault-spindown.
 *
 * openmediavault-spindown is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * openmediavault-spindown is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with openmediavault-spindown. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * The spindown time of a disk is OMV's own (conf.system.hdparm, the
 * Spindown time in Storage > Disks), so both pages always agree. Disks are
 * listed from sysfs only: listing them must never wake a sleeping disk.
 */
class OMVRpcServiceSpindown extends \OMV\Rpc\ServiceAbstract
{
	// Written by spindownd every poll; older than this means not running.
	const STATUS_FILE = "/run/spindownd/status.json";
	const STATUS_MAX_AGE = 120;

	public function getName()
	{
		return "Spindown";
	}

	public function initialize()
	{
		$this->registerMethod("getDisks");
		$this->registerMethod("getDisk");
		$this->registerMethod("setDisk");
	}

	/**
	 * One row per hard disk, with its spindown time and what spindownd
	 * last saw.
	 */
	public function getDisks($params, $context)
	{
		$this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
		$status = json_decode(@file_get_contents(self::STATUS_FILE) ?: "{}", TRUE);
		$watched = [];
		if (is_array($status) &&
				time() - ($status['timestamp'] ?? 0) < self::STATUS_MAX_AGE) {
			foreach ($status['disks'] ?? [] as $entry) {
				if (!empty($entry['device'])) {
					$watched[$entry['device']] = $entry;
				}
			}
		}
		$times = $this->spindownTimes();
		$rows = [];
		foreach ($this->hardDisks() as $name => $disk) {
			$value = $times[$name]['spindowntime'] ?? 0;
			$seconds = $this->standbySeconds($value);
			$entry = $watched[$name] ?? NULL;
			$rows[] = array_merge($disk, [
				"spindowntime" => $value,
				"sleepafter" => is_null($seconds) ? gettext("Never") :
					$this->duration($seconds),
				"statustext" => $this->statusText($seconds, $entry),
				"laststandby" => $entry['last_standby'] ?? NULL,
				"spindowns" => $entry['spindowns'] ?? 0,
			]);
		}
		return $rows;
	}

	/**
	 * The edit page: one disk and its spindown time.
	 */
	public function getDisk($params, $context)
	{
		$this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
		$this->validateMethodParams($params, "rpc.spindown.getdisk");
		$disk = $this->assertGetDisk($params['devicefile']);
		$times = $this->spindownTimes();
		$description = $disk['device'];
		if (!empty($disk['model'])) {
			$description .= " · " . $disk['model'];
		}
		if ($disk['size'] > 0) {
			$description .= " · " . binary_format($disk['size']);
		}
		return [
			"devicefile" => $disk['devicefile'],
			"description" => $description,
			"spindowntime" => $times[$disk['device']]['spindowntime'] ?? 0,
		];
	}

	/**
	 * Store the spindown time in the disk's Storage > Disks settings,
	 * creating them if the disk has none yet.
	 */
	public function setDisk($params, $context)
	{
		$this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
		$this->validateMethodParams($params, "rpc.spindown.setdisk");
		$disk = $this->assertGetDisk($params['devicefile']);
		$times = $this->spindownTimes();
		if (array_key_exists($disk['device'], $times)) {
			$settings = $times[$disk['device']];
		} else {
			$object = new \OMV\Config\ConfigObject("conf.system.hdparm");
			$object->set("uuid", \OMV\Environment::get("OMV_CONFIGOBJECT_NEW_UUID"));
			$object->set("devicefile", $disk['devicefile']);
			// OMV applies every field, so keep the disk's write cache as it
			// is now instead of the datamodel's "off".
			$object->set("writecache", $disk['writecache']);
			$settings = $object->getAssoc();
		}
		$settings['spindowntime'] = $params['spindowntime'];
		return \OMV\Rpc\Rpc::call("DiskMgmt", "setHdParm", $settings, $context);
	}

	/**
	 * Rotating disks by kernel name, from sysfs.
	 */
	private function hardDisks()
	{
		$disks = [];
		foreach (glob("/sys/block/*") as $path) {
			$name = basename($path);
			if (!preg_match('/^(sd|hd)[a-z]+$/', $name) ||
					trim(@file_get_contents("{$path}/queue/rotational")) !== "1") {
				continue;
			}
			$disks[$name] = [
				"device" => $name,
				"devicefile" => $this->stableDeviceFile($name),
				"model" => trim(@file_get_contents("{$path}/device/model") ?: ""),
				"size" => (int)trim(@file_get_contents("{$path}/size") ?: "0") * 512,
				"writecache" => trim(@file_get_contents(
					"{$path}/queue/write_cache") ?: "") === "write back",
			];
		}
		ksort($disks, SORT_NATURAL);
		return $disks;
	}

	/**
	 * A /dev/disk/by-id link for the disk, which survives reboots.
	 */
	private function stableDeviceFile($name)
	{
		$rank = ["ata-" => 0, "scsi-" => 1, "usb-" => 2, "wwn-" => 3];
		$best = NULL;
		$bestRank = PHP_INT_MAX;
		foreach (glob("/dev/disk/by-id/*") ?: [] as $link) {
			$base = basename($link);
			if (preg_match('/-part\d+$/', $base) ||
					realpath($link) !== "/dev/{$name}") {
				continue;
			}
			$linkRank = count($rank);
			foreach ($rank as $prefix => $value) {
				if (str_starts_with($base, $prefix)) {
					$linkRank = $value;
					break;
				}
			}
			if ($linkRank < $bestRank ||
					($linkRank == $bestRank && strcmp($link, $best) < 0)) {
				$best = $link;
				$bestRank = $linkRank;
			}
		}
		return $best ?? "/dev/{$name}";
	}

	private function assertGetDisk($devicefile)
	{
		$path = realpath($devicefile);
		$disks = $this->hardDisks();
		$name = $path ? basename($path) : "";
		if (!array_key_exists($name, $disks)) {
			throw new \OMV\Exception(gettext("Hard disk '%s' not found."),
				$devicefile);
		}
		return $disks[$name];
	}

	/**
	 * Storage > Disks settings by kernel name.
	 */
	private function spindownTimes()
	{
		$db = \OMV\Config\Database::getInstance();
		$times = [];
		foreach ($db->get("conf.system.hdparm") as $object) {
			$path = realpath($object->get("devicefile"));
			if ($path && !array_key_exists(basename($path), $times)) {
				$times[basename($path)] = $object->getAssoc();
			}
		}
		return $times;
	}

	private function statusText($seconds, $entry)
	{
		if (is_null($seconds)) {
			return gettext("Stays awake");
		}
		if (is_null($entry)) {
			return gettext("Not watched yet: apply the pending changes");
		}
		$idle = $entry['idle'] ?? 0;
		$left = ($entry['timeout'] ?? $seconds) - $idle;
		switch ($entry['state'] ?? "") {
		case "standby":
			return gettext("Asleep");
		case "missing":
			return gettext("Not connected");
		case "failed":
			return gettext("Does not accept the sleep command");
		case "active":
			if ($idle < 60) {
				return gettext("In use");
			}
		}
		if ($left < 60) {
			return gettext("Going to sleep");
		}
		return sprintf(gettext("Idle, sleeps in %s"), $this->duration($left));
	}

	/**
	 * Seconds of an ATA standby timer value, as in spindownd.
	 */
	private function standbySeconds($value)
	{
		$value = (int)$value;
		if ($value >= 1 && $value <= 240) {
			return $value * 5;
		}
		if ($value >= 241 && $value <= 251) {
			return ($value - 240) * 1800;
		}
		if ($value == 252) {
			return 21 * 60;
		}
		if ($value == 255) {
			return 21 * 60 + 15;
		}
		return NULL;
	}

	private function duration($seconds)
	{
		$minutes = intdiv((int)$seconds, 60);
		if ($minutes < 1) {
			return gettext("under a minute");
		}
		if ($minutes < 120) {
			return sprintf(gettext("%d min"), $minutes);
		}
		if ($minutes % 60 == 0) {
			return sprintf(gettext("%d hours"), intdiv($minutes, 60));
		}
		return sprintf(gettext("%d h %d min"), intdiv($minutes, 60),
			$minutes % 60);
	}
}
