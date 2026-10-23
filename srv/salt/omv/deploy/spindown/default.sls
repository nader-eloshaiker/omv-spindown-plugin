# This file is part of openmediavault-spindown.
#
# openmediavault-spindown is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# openmediavault-spindown is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with openmediavault-spindown. If not, see <http://www.gnu.org/licenses/>.

# The spindown time of each disk is the one set in Storage > Disks or on
# the Spin down page (the same setting smartctl-hdparm gives the disk's own
# timer). The daemon runs while at least one disk has one.
{% set hdparm_config = salt['omv_conf.get']('conf.system.hdparm') %}
{% set disks = [] %}
{% for device in hdparm_config %}
{% if device.spindowntime | int > 0 %}
{% set _ = disks.append({'devicefile': device.devicefile,
  'spindowntime': device.spindowntime | int}) %}
{% endif %}
{% endfor %}

configure_spindownd:
  file.managed:
    - name: "/etc/spindownd/spindownd.json"
    - source:
      - salt://{{ tpldir }}/files/etc-spindownd-spindownd_json.j2
    - template: jinja
    - context:
        config: {{ {'enable': disks | length > 0, 'disks': disks} | json }}
    - makedirs: True
    - user: root
    - group: root
    - mode: '0644'

{% if disks | length > 0 %}

start_spindownd_service:
  service.running:
    - name: spindownd
    - enable: True
    - watch:
      - file: configure_spindownd

{% else %}

stop_spindownd_service:
  service.dead:
    - name: spindownd
    - enable: False

{% endif %}
