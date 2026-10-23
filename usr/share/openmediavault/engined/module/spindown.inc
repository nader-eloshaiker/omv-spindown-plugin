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

class OMVModuleSpindown extends \OMV\Engine\Module\ServiceAbstract
	implements \OMV\Engine\Notify\IListener, \OMV\Engine\Module\IServiceStatus
{
	/**
	 * Must match the salt state directory srv/salt/omv/deploy/spindown.
	 */
	public function getName()
	{
		return "spindown";
	}

	public function getDescription()
	{
		return gettext("Spin down idle disks that ignore their standby timer.");
	}

	public function getStatus()
	{
		$db = \OMV\Config\Database::getInstance();
		$object = $db->get("conf.service.spindown");
		$systemCtl = new \OMV\System\SystemCtl("spindownd");
		return [
			"name" => $this->getName(),
			"title" => gettext("Disk spin down"),
			"enabled" => $object->get("enable"),
			"running" => $systemCtl->isActive()
		];
	}

	public function bindListeners(\OMV\Engine\Notify\Dispatcher $dispatcher)
	{
		$dispatcher->addListener(OMV_NOTIFY_MODIFY,
			"org.openmediavault.conf.service.spindown", [$this, "setDirty"]);
		// The spindown times come from Storage > Disks.
		$dispatcher->addListener(
			OMV_NOTIFY_CREATE | OMV_NOTIFY_MODIFY | OMV_NOTIFY_DELETE,
			"org.openmediavault.conf.system.hdparm", [$this, "setDirty"]);
	}
}
