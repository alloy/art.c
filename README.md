# ArtC

Artsy in C major

https://twitter.com/alloy/status/613375165582417922

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

1. Install ngrok from https://dashboard.ngrok.com/get-started

1. Start ngrok:

   ```bash
   $ ngrok http 8080
   ```

1. Start the server:

   ```bash
   $ rake -s
   ```

# Development

1. Install `clang-format`:

   ```bash
   $ brew install clang-format
   ```

1. Bookmark https://silverhammermba.github.io/emberb/c/

1. Perform request from fixture:

   ```bash
   $ curl -i -X POST --data @fixtures/page.json http://localhost:8080/webhooks/analytics
   ```
