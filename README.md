Artsy in C

# Installation

1. On macOS you can use the system Ruby, but if you prefer a newer version of Ruby install one _with_ a dynamic (shared)
   library product. E.g.:

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
