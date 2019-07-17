# art.c: The Artsy universe in C major and C Ruby.

This program plays musical notes in the major C scale in near real-time as users interact with artsy.net. If you are
lucky enough–or are actually bidding/offering/buying a work while reading this 🤘–you may even hear a bell being struck.

## Why?

This is my entry for the 2019 edition of the Artsy Salon, an annual art exhibition in which Artsy employees show-case
their work.

Artsy historically uses the Ruby programming language a lot, as such using it’s VM (virtual machine) here is solely out
of sentiment about Artsy’s platform origins and as a tribute to my life-long love for Ruby. However, being inspired by
Ruby’s long-lost artist ‘[\_why the lucky stiff][_why]’ I couldn’t simply write Ruby, instead the entire Ruby program is
written in the C programming language using Ruby’s C programming interface.

There is no good reason to do this, other than to question yourself how things really work. Taking the mechanics of a
‘reality’ (such as a VM) for granted is limiting to one’s understanding of the space it lives in.

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

1. If actually consuming events from a segment.com webhook, start a forwarding tunnel from serveo.net:

   ```bash
   $ ssh -R my-example-subdomain:80:localhost:8080 serveo.net
   ```

1. …and point the segment.com webhook to it. E.g. `https://my-example-subdomain.serveo.net/webhooks/analytics`

1. When letting this run for extended periods, it is best to use `autossh` as it will automatically reconnect.

   ```bash
   $ brew install autossh
   $ autossh -M 0 -R my-example-subdomain:80:localhost:8080 serveo.net
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
  - `rb_p(…)` is your `p` friend that you can use to inspect `VALUE` objects, i.e. Ruby objects

- Perform request from fixture:

  ```bash
  $ curl -i -X POST --data @fixtures/page.json http://localhost:8080/webhooks/analytics
  ```

[coreaudio]: https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/CoreAudioOverview/WhatisCoreAudio/WhatisCoreAudio.html
[coremidi]: https://developer.apple.com/documentation/coremidi?language=objc
[gcd]: https://apple.github.io/swift-corelibs-libdispatch/
[_why]: https://en.wikipedia.org/wiki/Why_the_lucky_stiff
[mri]: https://en.wikipedia.org/wiki/Ruby_MRI
