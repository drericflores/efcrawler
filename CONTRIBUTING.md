# Contributing to eFCrawler

Thank you for your interest in eFCrawler. This document explains the best way
to report a problem, ask a question, or propose a change.

eFCrawler is maintained as an independent project. Clear, well-scoped reports
are the most valuable contribution you can make.

---

## Reporting a bug

Open an issue using the **Bug report** template:

https://github.com/drericflores/efcrawler/issues/new/choose

The template asks for your version, distribution, Qt version, which research
provider was active, and whether free-resource filtering was on. These are not
bureaucratic formalities — eFCrawler depends on external providers, so a
research failure has several possible causes, and these details are what
distinguish them.

### What makes a good report

| Include | Why it matters |
| --- | --- |
| eFCrawler version | Behaviour changes between releases |
| Distribution and Qt version | Qt version differences affect networking and the build |
| The research provider in use | DuckDuckGo and Wikipedia fail in different ways |
| Whether free filtering was on | It suppresses results by design, so it changes what you should expect |
| Terminal output | Errors and warnings print there, not in the interface |
| Exact steps | A reproducible sequence is worth more than a description |

### Please do not include

- Private or personal research topics
- Downloaded documents themselves (link to the source instead)
- API keys, tokens, or credentials

---

## Reporting a security issue

Please do not report security problems in a public issue.

If private vulnerability reporting is enabled on this repository, use it via
the **Security** tab. Otherwise, contact the maintainer directly so the problem
can be assessed before it is disclosed publicly.

---

## Asking a question

Search the existing issues and the README first. If your question is not
answered there, open an issue and describe what you have already tried.

---

## Proposing a feature

Open an issue using the **Feature request** template. Describe the problem you
are running into before describing the solution you have in mind — the problem
is usually the more useful half, and it leaves room for an approach neither of
us has considered.

Before proposing a large change, please open the issue first and wait for a
response. That avoids work being done on something that does not fit the
project's direction.

---

## Contributing code

### Building from source

See the [Build](README.md#build) section of the README. The requirements are a
C++23-capable compiler, CMake, and the Qt6 Widgets and Network modules.

```bash
sudo apt install build-essential cmake qt6-base-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

### Guidelines

- **Keep changes scoped.** One fix or feature per pull request.
- **Match the surrounding code.** Follow the existing style of the file you are
  editing rather than introducing a new one.
- **Explain the why.** A pull request description should say what problem the
  change solves, not only what it does.
- **Test the build.** Confirm `cmake --build build` succeeds, and run the
  application through the path your change affects.
- **Update the documentation.** If behaviour or configuration changes, the
  README or this file may need a matching edit.

### Research providers

eFCrawler uses a provider-based architecture. A new provider implements the
`SearchProvider` interface and is registered with the provider manager; the
research engine and the interface do not need to change.

Provider implementations that rely on documented APIs are strongly preferred
over ones that parse HTML, as scraped interfaces change without notice and can
stop working without any change on our side.

---

## Licence

By contributing, you agree that your contributions are licensed under the
GNU General Public License, version 3 or later — the same licence as the
project. See [LICENSE](LICENSE).

---

## Thank you

Every clear report, corrected typo, and considered suggestion makes eFCrawler
better for everyone using it for research and learning.
