<p align="center">
<a href="https://github.com/onesoft-sudo/freehttpd" title="freehttpd">
<img src="https://raw.githubusercontent.com/onesoft-sudo/freehttpd/refs/heads/main/res/freehttpd.png" height="152px" width="360px">
</a> 
</p>

<p align="center">
freehttpd is a lightweight and high-performance HTTP server, written in C.
</p>

## Installation

#### Requirements

- GCC 14+ or Clang 19+
- OpenSSL
- libsystemd (if using systemd)

#### Building from source

Clone this repository:

```bash
git clone https://github.com/onesoft-sudo/freehttpd.git
```

Then use `make` to build:

```bash
cd freehttpd/
make
```

This should create a `bin/` directory, which should contain all programs necessary to operate freehttpd.

## Licensing

freehttpd is licensed under the [GNU Affero General Public License v3.0](https://gnu.org/licenses/agpl-3.0.html). You can find the full license text in the [COPYING](./COPYING) file.

![AGPL-3.0](https://www.gnu.org/graphics/agplv3-155x51.png)