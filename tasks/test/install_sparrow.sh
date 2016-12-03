opkg -f /etc/opkg/opkg.conf update && \
opkg -f /etc/opkg/opkg.conf  install bash curl   && \
curl -k -L https://cpanmin.us -o /usr/bin/cpanm && \
chmod +x /usr/bin/cpanm && \
ln -fs /bin/env /usr/bin/env && \
cpanm --notest -q Sparrow && \
sparrow index update
