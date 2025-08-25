# Package building with nFPM
.PHONY: packages deb rpm clean-packages install-nfpm

# Install nFPM if not present
install-nfpm:
	@if ! command -v nfpm >/dev/null 2>&1; then \
		echo "Installing nFPM..."; \
		echo 'deb [trusted=yes] https://repo.goreleaser.com/apt/ /' | sudo tee /etc/apt/sources.list.d/goreleaser.list; \
		sudo apt update; \
		sudo apt install nfpm; \
	fi

# Build binaries first
build:
	cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
	cmake --build build --parallel

# Build all packages
packages: install-nfpm build
	@echo "Building packages..."
	mkdir -p dist
	nfpm package --packager deb --target dist/pwar.deb --config nfpm.yaml
	nfpm package --packager rpm --target dist/pwar.rpm --config nfpm.yaml
	@echo "Packages built in dist/ directory"

# Build only DEB package
deb: install-nfpm build
	@echo "Building DEB package..."
	mkdir -p dist
	nfpm package --packager deb --target dist/pwar.deb --config nfpm.yaml

# Build only RPM package  
rpm: install-nfpm build
	@echo "Building RPM package..."
	mkdir -p dist
	nfpm package --packager rpm --target dist/pwar.rpm --config nfpm.yaml

# Clean package artifacts
clean-packages:
	rm -rf dist/

# Test install locally (requires sudo)
test-install-deb: deb
	sudo dpkg -i dist/pwar.deb

test-install-rpm: rpm
	sudo rpm -i dist/pwar.rpm
