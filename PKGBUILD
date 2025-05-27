pkgname=pandaforever
pkgver=1
pkgrel=1
pkgdesc="exhentai downloader"
license=('GPL3')
arch=('any')
depends=(curl)

source=(git+https://gitlab.com/squishydreams/pandaforever.git)
sha256sums=('SKIP')

pkgver() {
	cd pandaforever
	printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
	cd pandaforever
	make
}
package() {
	cd pandaforever
	make PREFIX=/usr DESTDIR="${pkgdir}" install
}
