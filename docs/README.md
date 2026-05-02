# CXXExtension

CXXExtension is a C++23 modules library with small utilities for errors, parsing,
strings, containers, and actor-style message processing. Actors can be pumped
manually, started on an owned worker thread, and used with one-shot replies.

Import the umbrella module when you want the whole library:

```cpp
import CXXExtension;
```

Use focused modules when you want a smaller surface:

```cpp
import CXXExtension.Core;
import CXXExtension.Parse;
import CXXExtension.String;
import CXXExtension.Container;
import CXXExtension.ContainerExtension;
```

## Guides

- [Getting Started](GettingStarted.md)
- [Core](Core.md)
- [Parsing And Strings](ParsingAndStrings.md)
- [Containers](Containers.md)
- [Actors](Actors.md)

The source comments are the API reference. These guide pages are intentionally
tutorial-oriented.
