FROM ubuntu:latest

RUN apt-get update && apt-get install -y samba procps

COPY smb.conf /etc/samba/smb.conf

RUN mkdir -p /libraryit/ebooks /libraryit/papers /libraryit/sourcecode /libraryit/docs

RUN printf '#!/bin/bash\n\
groupadd staff 2>/dev/null || true\n\
groupadd readonly 2>/dev/null || true\n\
id -u member &>/dev/null || useradd -m -G readonly member\n\
id -u contributor &>/dev/null || useradd -m -G staff contributor\n\
id -u librarian &>/dev/null || useradd -m -G staff librarian\n\
echo "member:member123" | chpasswd\n\
echo "contributor:contrib456" | chpasswd\n\
echo "librarian:lib789" | chpasswd\n\
(echo "member123"; echo "member123") | smbpasswd -a -s member\n\
(echo "contrib456"; echo "contrib456") | smbpasswd -a -s contributor\n\
(echo "lib789"; echo "lib789") | smbpasswd -a -s librarian\n\
chmod -R 777 /libraryit/\n\
# Jalankan smbd secara interaktif tanpa menjadi daemon sama sekali\n\
exec smbd -i -F -d 1 --no-process-group\n' > /init.sh && chmod +x /init.sh

ENTRYPOINT ["/bin/bash", "/init.sh"]
