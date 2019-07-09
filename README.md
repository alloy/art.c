# art.c

Activity in the Artsy universe observed through audible means in the C major scale and implemented in the C programming
language–using the Ruby runtime for no reason other than sentiment about Artsy’s platform origins.

All of Artsy’s analytics events go through segment.com, which in turn sends those aggregated events to the HTTP server
in this project so it can turn those events into an audible representation.

This is my entry for the 2019 edition of the Artsy Salon, an annual art exhibition in which Artsy employees show-case
their work.

# Installation

1. On macOS you can use the system Ruby, but if you prefer a newer version of
   Ruby install one _with_ a dynamic (shared) library product. E.g.:

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

- Most-all communication in the app must be done through the Ruby runtime. I.e. rather than a C function call, expose
  the C function as a Ruby method an invoke that instead.

- Install `clang-format`:

  ```bash
  $ brew install clang-format
  ```

- C Ruby reading:

  - https://github.com/ruby/ruby
  - https://silverhammermba.github.io/emberb/c/

- Perform request from fixture:

  ```bash
  $ curl -i -X POST --data @fixtures/page.json http://localhost:8080/webhooks/analytics
  ```
