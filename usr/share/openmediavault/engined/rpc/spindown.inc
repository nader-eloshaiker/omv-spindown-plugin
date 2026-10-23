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
		$this->registerMethod("getSettings");
		$this->registerMethod("setSettings");
		$this->registerMethod("getDisks");
	}

	public function getSettings($params, $context)
	{
		$this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
		$db = \OMV\Config\Database::getInstance();
		$result = $db->getAssoc("conf.service.spindown");
		$disks = array_map(function($disk) {
			return sprintf("%s (%s)", $disk['name'] ?? basename($disk['devicefile']),
				$this->duration($disk['seconds']));
		}, $this->configuredDisks());
		$result['disks'] = !empty($disks) ? implode(", ", $disks) :
			gettext("None yet: set a Spindown time in Storage › Disks.");
		return $result;
	}

	public function setSettings($params, $context)
	{
		$this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
		$this->validateMethodParams($params, "rpc.spindown.setsettings");
		$db = \OMV\Config\Database::getInstance();
		$object = $db->get("conf.service.spindown");
		$object->setAssoc($params);
		$db->set($object);
		return $object->getAssoc();
	}

	/**
	 * Disks page: one row per disk with a spindown time, from what the
	 * daemon last saw, or from the settings while it is not running.
	 */
	public function getDisks($params, $context)
	{
		$this->validateMethodContext($context, ["role" => OMV_ROLE_ADMINISTRATOR]);
		$states = [
			"active" => gettext("Awake (in use)"),
			"standby" => gettext("Asleep"),
			"unknown" => gettext("Awake, waiting"),
			"missing" => gettext("Not connected"),
			"failed" => gettext("Does not accept standby"),
		];
		$status = json_decode(@file_get_contents(self::STATUS_FILE) ?: "{}", TRUE);
		$db = \OMV\Config\Database::getInstance();
		$running = $db->get("conf.service.spindown")->get("enable") &&
			is_array($status) &&
			time() - ($status['timestamp'] ?? 0) < self::STATUS_MAX_AGE;
		$rows = [];
		if ($running) {
			foreach ($status['disks'] ?? [] as $disk) {
				$rows[] = [
					"devicefile" => $disk['devicefile'],
					"device" => $disk['device'] ?? "",
					"model" => $disk['model'] ?? "",
					"timeout" => $this->duration($disk['timeout']),
					"idle" => is_null($disk['idle'] ?? NULL) ? "" :
						$this->duration($disk['idle']),
					"statetext" => $states[$disk['state']] ?? $disk['state'],
					"laststandby" => $disk['last_standby'] ?? NULL,
					"spindowns" => $disk['spindowns'] ?? 0,
				];
			}
			return $rows;
		}
		foreach ($this->configuredDisks() as $disk) {
			$rows[] = [
				"devicefile" => $disk['devicefile'],
				"device" => $disk['name'] ?? "",
				"model" => $disk['model'] ?? "",
				"timeout" => $this->duration($disk['seconds']),
				"idle" => "",
				"statetext" => gettext("Not watched: spin down is off"),
				"laststandby" => NULL,
				"spindowns" => 0,
			];
		}
		return $rows;
	}

	/**
	 * Disks with a spindown time in Storage > Disks.
	 */
	private function configuredDisks()
	{
		$db = \OMV\Config\Database::getInstance();
		$disks = [];
		foreach ($db->get("conf.system.hdparm") as $object) {
			$seconds = $this->standbySeconds($object->get("spindowntime"));
			$devicefile = $object->get("devicefile");
			if (is_null($seconds) || empty($devicefile)) {
				continue;
			}
			$path = realpath($devicefile);
			$name = $path ? basename($path) : NULL;
			$model = $name ? trim(@file_get_contents(
				"/sys/block/{$name}/device/model") ?: "") : "";
			$disks[] = ["devicefile" => $devicefile, "seconds" => $seconds,
				"name" => $name, "model" => $model];
		}
		return $disks;
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
		return sprintf(gettext("%d h %d min"), intdiv($minutes, 60),
			$minutes % 60);
	}
}
