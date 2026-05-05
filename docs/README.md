# IXXExtension

IXXExtension is a C++23 modules library with small utilities for errors, parsing,
strings, containers, and concurrency. Concurrency helpers include one-shot
channels, unbounded channels, and actor-style message processing.

Import the umbrella module when you want the whole library:

```cpp
import IXXExtension;
```

Use focused modules when you want a smaller surface:

```cpp
import IXXExtension.Core;
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
- [Containers](Containers.md)
- [Actors](Actors.md)

The source comments are the API reference. These guide pages are intentionally
tutorial-oriented.
