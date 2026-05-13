#!/bin/bash

groupadd staff 2>/dev/null || true
groupadd readonly 2>/dev/null || true

id -u member &>/dev/null || useradd -m -G readonly member
id -u contributor &>/dev/null || useradd -m -G staff contributor
id -u librarian &>/dev/null || useradd -m -G staff librarian

echo "member:member123" | chpasswd
echo "contributor:contrib456" | chpasswd
echo "librarian:lib789" | chpasswd

(echo "member123"; echo "member123") | smbpasswd -a -s member
(echo "contrib456"; echo "contrib456") | smbpasswd -a -s contributor
(echo "lib789"; echo "lib789") | smbpasswd -a -s librarian

chmod -R 777 /libraryit/

exec smbd -F -i -d 1 --no-process-group
