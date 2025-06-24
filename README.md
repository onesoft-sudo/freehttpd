<p align="center">
<a href="https://github.com/onesoft-sudo/freehttpd" title="freehttpd">
<img src="https://raw.githubusercontent.com/onesoft-sudo/freehttpd/refs/heads/main/res/freehttpd_http.png" height="152px" width="360px">
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

Clone this repository and run `./bootstrap` (not required if you're using a source tarball):

```bash
git clone https://github.com/onesoft-sudo/freehttpd.git
cd freehttpd/
./bootstrap
```

Then run the build:

```bash
./configure             # Use `--help' to see the available build options
make
make install
```

This should install all programs necessary to operate freehttpd, in `/usr/local`. You can change the destination by using the `--prefix` option with `./configure`.

## Licensing

freehttpd is licensed under the [GNU Affero General Public License v3.0](https://gnu.org/licenses/agpl-3.0.html). You can find the full license text in the [COPYING](./COPYING) file.

![AGPL-3.0](https://www.gnu.org/graphics/agplv3-155x51.png)
