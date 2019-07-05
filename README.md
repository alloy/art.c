Artsy in C

# Installation

1. Install `pkg-config`:

   ```bash
   $ brew install pkg-config
   ```

1. Install Ruby _with_ a dynamic (shared) library product. E.g.:

   ```bash
   $ ruby-install ruby 2.6.3 -- --enable-shared
   ```

1. Install Bundler, if necessary:

   ```bash
   $ gem install bundler
   ```

1. Install project gems:

   ```bash
   $ bundle install
   ```

# Run

```bash
$ rake -s
```

# Development

1. Install `clang-format`:

   ```bash
   $ brew install clang-format
   ```
