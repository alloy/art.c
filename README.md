# art.c

Activity in the Artsy universe observed through audible means in the C major scale and implemented in the C programming
language using the Ruby runtime API.

## Why?

This is my entry for the 2019 edition of the Artsy Salon, an annual art exhibition in which Artsy employees show-case
their work.

Artsy historically uses a lot of Ruby and so using it’s runtime here is solely out of sentiment about Artsy’s platform
origins and as a tribute to my life-long love for Ruby. To be clear, other than in the `Rakefile`, all Ruby is written
using the C APIs of [Matz’s Ruby Interpreter][mri]. It doesn't need to make sense, this is an art piece after all.

Finally, both as an homage and inspiration, I tried to use “What would [_why] do?” as a guiding principle.

## How?

All of Artsy’s analytics events go through segment.com, which in turn sends those aggregated events to the HTTP server
in this project so it can turn those events into an audible representation.

The HTTP server is a plain Ruby Rack app, except using the C Ruby API, and the sound is produced using a [CoreAudio] and
[CoreMIDI] stack that runs in a [Grand Central Dispatch][gcd] background thread to which the app enqueues notes to play
based on the type of event that occurred.

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

1. Start the server, which is enough for local development work:

   ```bash
   $ rake -s
   ```

1. If actually consuming events from a segment.com webhook, start ngrok:

   ```bash
   $ ngrok http 8080
   ```

1. …and point the segment.com webhook to the published ngrok URL.

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

[coreaudio]: https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/CoreAudioOverview/WhatisCoreAudio/WhatisCoreAudio.html
[coremidi]: https://developer.apple.com/documentation/coremidi?language=objc
[gcd]: https://apple.github.io/swift-corelibs-libdispatch/
[_why]: https://en.wikipedia.org/wiki/Why_the_lucky_stiff
[mri]: https://en.wikipedia.org/wiki/Ruby_MRI
