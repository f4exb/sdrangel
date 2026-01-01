---
applyTo: '**/*.cpp, **/*.h, **/*.hpp, **/*.cxx, **/*.cc'
---
# Qt C++ & SDRangel App coding conventions and guidelines

These instructions guide Copilot to generate code that aligns with modern Qt C++ standards, C++20/23 features, software engineering principles, and industry best practices to improve software quality, maintainability, and performance for the SDRangel application.

## ✅ General C++ Coding Standards

- Follow **Google C++ Style Guide** or **Core Guidelines** for consistent code structure
- Use **meaningful, descriptive variable, function, class, and file names**
- Apply proper Doxygen comments for classes, methods, and complex logic
- Organize code into small, reusable functions or classes with single responsibility
- Avoid magic numbers or hard-coded strings; use constants, enums, or configuration
- Use **RAII** (Resource Acquisition Is Initialization) for all resource management
- Prefer **const correctness** throughout the codebase

## ✅ Modern C++20/23 Best Practices

- Use **auto** for type deduction when it improves readability
- Utilize **smart pointers** (`std::unique_ptr`, `std::shared_ptr`) over raw pointers
- Apply **range-based for loops** instead of traditional iterator loops
- Use **constexpr** for compile-time constants and functions
- Leverage **structured bindings** for tuple/pair unpacking
- Use **std::optional** for optional values instead of null pointers
- Apply **concepts** for template constraints where applicable
- Use **std::span** for safe array/container views
- Utilize **std::string_view** for read-only string parameters
- Apply **designated initializers** for struct initialization
- Use **[[nodiscard]]** attribute for functions whose return values shouldn't be ignored

## ✅ Qt Framework Standards & Conventions

- Follow Qt's **CamelCase** naming convention for classes and **camelCase** for methods/variables
- Use Qt's **parent-child ownership model** for automatic memory management
- Apply **Q_OBJECT** macro for classes that need signals/slots or meta-object features
- Use **signals and slots** for loose coupling between components
- Prefer **Qt containers** (`QVector`, `QHash`, `QSet`) over STL containers when integrating with Qt APIs
- Use **Qt's foreach** or range-based for loops with Qt containers
- Apply **Qt's string handling** (`QString`, `QStringView`) for Unicode support
- Use **QPointer** for safe weak references to QObjects

## ✅ Project Structure & File Organization

- Header files should:
  - Use include guards
  - Forward declare classes when possible
  - Separate public, protected, and private sections clearly
- Source files should:
  - Include corresponding header files first
  - Group includes: standard library, Qt headers, project headers
- Organize classes into namespaces when necessary to avoid name collisions
- Maintain a clear separation between interface (header) and implementation (source) files

## ✅ Qt-Specific Patterns for SDRangel

- **Main Application Structure**:

  - Implement proper application lifecycle management
  - Use `QSettings` for persistent configuration storage

- **Data Collection & Monitoring**:

  - Use `QTimer` for periodic data collection
  - Apply `QThread` or `QRunnable` for background processing
  - Use `QMutex` or `QReadWriteLock` for thread synchronization
  - Implement `QFileSystemWatcher` for file monitoring

- **UI Components**:

  - Use `QMainWindow` as the primary window class
  - Apply `QWidget` customization for specialized displays
  - Use `QCharts` for data visualization
  - Implement `QSystemTrayIcon` for background operation
  - Use `QDialog` for settings and configuration windows

- **Networking & API Integration**:
  - Use `QNetworkAccessManager` for HTTP requests
  - Apply `QNetworkReply` for handling responses
  - Use `QJsonDocument` and `QJsonObject` for JSON parsing
  - Implement proper SSL/TLS handling with `QSslConfiguration`

## ✅ Performance & Memory Management

- Use **Qt's object ownership** system to prevent memory leaks
- Apply **lazy initialization** for expensive resources
- Use **QObject::deleteLater()** for safe object deletion
- Implement **efficient string operations** with `QStringBuilder`
- Use **QCache** for frequently accessed data
- Apply **QPixmapCache** for image caching
- Optimize **paint events** with proper clipping and caching

## ✅ Cross-Platform Development

- Use **Qt's platform abstraction** instead of platform-specific code
- Apply **conditional compilation** sparingly with `Q_OS_*` macros
- Use **QStandardPaths** for platform-appropriate file locations
- Implement **platform-specific features** through Qt's platform plugins
- Apply **High DPI scaling** support with `Qt::AA_EnableHighDpiScaling`

## ✅ Modern Qt Features to Leverage

- Use **Qt Quick** for modern, fluid UIs where appropriate
- Apply **Qt Concurrent** for parallel processing
- Use **Qt State Machine** for complex state management
- Apply **Qt's logging framework** with categories
- Use **Qt's property system** for dynamic behavior
- Apply **Qt's animation framework** for smooth transitions

## ✅ Code Quality & Maintainability

- Follow **SOLID Principles** adapted for Qt:

  - Single Responsibility: Each class has one clear purpose
  - Open/Closed: Use Qt's plugin system for extensions
  - Liskov Substitution: Proper inheritance hierarchies
  - Interface Segregation: Use Qt's abstract base classes
  - Dependency Inversion: Use Qt's dependency injection patterns

- Apply **Qt's Model-View patterns** for data presentation
- Use **Qt's undo framework** for user actions
- Implement **proper error handling** with Qt's exception safety
- Apply **const correctness** especially with Qt's implicit sharing

## ✅ Additional Copilot Behavior Preferences

- Generate **const-correct**, modern C++ code using Qt idioms
- Prioritize **readable, maintainable** code over premature optimization
- Avoid deprecated Qt APIs; use modern alternatives
- Suggest proper class placement based on Qt application structure
- Default to **Qt's memory management** and **RAII** patterns
- Include **thread safety considerations** for multi-threaded operations
- Consider **platform differences** when suggesting system-level code
- Provide **Doxygen-style documentation** for complex functions
- Suggest **unit tests** alongside new features where applicable
- Use **Qt's internationalization** support for user-facing strings
- Be direct and concise in code comments; avoid unnecessary verbosity
- Avoid starting responses with "Sure!", "You're right!" or similar phrases; be direct and concise.
- When writing text that'll be visible to users, use **clear, professional language** without unnecessary exclamations or informalities. Also use Sentence case for titles and headings.
