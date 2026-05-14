# IXXExtension

IXXExtension is a C++23 modules library with small utilities for errors,
parsing, strings, UTF-8 text handling, containers, jobs, and concurrency.
Concurrency helpers include one-shot channels, unbounded channels, actor-style
message processing, and a fixed-size thread pool.

Import the umbrella module when you want the whole library:

```cpp
import IXXExtension;
```

Use focused modules when you want a smaller surface:

```cpp
import IXXExtension.Core;
import IXXExtension.Jobs;
import IXXExtension.Text;
import IXXExtension.Parse;
import IXXExtension.String;
import IXXExtension.Collections;
import IXXExtension.Concurrency;
import IXXExtension.ContainerExtension;
```

## Guides

- [Getting Started](GettingStarted.md)
- [Core](Core.md)
- [Parsing And Strings](ParsingAndStrings.md)
- [Text](Text.md)
- [Containers](Containers.md)
- [Actors](Actors.md)
- [Jobs](Jobs.md)

The source comments are the API reference. These guide pages are intentionally
tutorial-oriented.

Standalone checkouts also expose the `examples/` files as xmake targets. Use
`xmake -g examples` to build all examples, or `xmake run example_text_utf8` to
run one target.
