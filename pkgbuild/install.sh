cd /tmp
git clone https://github.com/Mediocre-Games-United/mcc.git
cd mcc/pkgbuild
makepkg -si
rm -rf /tmp/mcc
