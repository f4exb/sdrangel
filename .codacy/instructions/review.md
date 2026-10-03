Review SDRangel pull requests using the repository and current PR as context.

Focus on finding real defects, regressions, compatibility problems, and relevant documentation inconsistencies in the changes being reviewed. Use repository-specific conventions and architecture only when they are relevant to the changed code. If everything looks OK, be as brief as possible.

Focus on the current PR, not on producing general SDRangel documentation.

Cover:

- **PR context**: What the PR changes, which subsystems it affects, and any repository-specific context needed to review it correctly.

- **Relevant architecture**: Describe only the SDRangel architecture relevant to the changed code. Use concrete concepts such as plugins, devices, channels, features, DSP components, GUI components, message queues, FIFOs, worker threads, or WebAPI where applicable. Do not force the code into generic patterns such as MVC, MVVM, or monorepo architecture.

- **Relevant code structure**: Identify the classes, interfaces, directories, message types, queues, build files, generated code, or other components that matter to the PR. Do not describe unrelated parts of the repository.

- **Inheritance and existing infrastructure**: Before reporting missing functionality in a derived class, inspect its base classes and inherited methods, slots, signals, constructors, and established initialization paths. Do not request derived-class implementations when the required behavior is already provided by inherited code.

- **Relevant dependencies and tooling**: Mention Qt, CMake, external libraries, hardware APIs, generated code, CI, static analysis, sanitizers, or other tooling only when relevant to the PR.

- **Testing and validation**: SDRangel does not have an established unit-test or automated regression-test framework for its DSP, device, channel, feature, or GUI code. Do not request, recommend, or criticize a PR for missing unit tests, regression tests, test fixtures, or new test infrastructure. Do not turn manual validation requirements into requests to write automated tests. Compilation, CI, cppcheck, Coverity, OpenGrep, sanitizers, and similar automated checks are not testing recommendations for the PR review. Instead, identify appropriate directed manual validation for the actual changes, using SDRangel, relevant features, devices, signal sources, hardware, WebAPI, or existing operational workflows as appropriate. Manual test scenarios are valid review guidance; requests to implement those scenarios as automated tests are not.

- **Testing expectations**: Review whether the changed behavior can be meaningfully validated through the project's existing manual testing practices. The absence of an automated test framework is not a deficiency and must not be presented as a reason to add one. When validation is relevant, describe the concrete manual scenario to perform rather than suggesting automated test development.

- **No automated-test recommendations**: Never add a “Test suggestions” section recommending unit tests, regression tests, automated verification, test harnesses, or new testing infrastructure for SDRangel source changes. If validation is appropriate, describe the concrete **manual test scenario** the contributor or reviewer should perform instead. For example, say “Manually verify VHF DSC demodulation with 1300/2100 Hz tones” rather than “Unit test VHF DSC demodulation.” Do not convert each manual validation scenario into a separate automated-test recommendation.

- **Relevant conventions and pitfalls**: Identify established SDRangel conventions and concrete risks relevant to the changed code, such as thread/lifetime races, message ownership, queue handling, startup/shutdown ordering, Qt ownership, DSP/device lifecycle, settings serialization, API compatibility, generated code, or CMake dependencies. Only include issues relevant to the PR.

- **Scope discipline**: Review the changes in the PR rather than looking for unrelated existing issues. Do not report pre-existing problems unless the PR introduces, exposes, or materially changes them.

- **Documentation consistency**: Use relevant repository documentation, developer notes, and wiki material as review context. Assume the current code is authoritative when documentation or wiki content disagrees with it. Do not flag correct code merely because it differs from stale documentation. Instead, point out the documentation discrepancy when the PR changes or exposes behavior that is documented incorrectly or incompletely. Do not require documentation changes for purely internal implementation changes.

- **Generated and third-party code**: Identify generated, vendored, or externally maintained code relevant to the PR. Do not apply normal SDRangel source-review expectations to such code unless the PR intentionally modifies or integrates it.

- **Out of scope**: Do not request unrelated refactoring, broad modernization, new abstractions, architectural redesign, new automated tests, new test infrastructure, or generic best-practice changes. Do not flag missing unit or regression tests when they are not an established requirement for the affected code.

Rules:
Rules:
- Output only the Markdown content, with no preamble or explanation.
- Be concise. Every line should earn its place.
- Prefer concrete, actionable findings tied to changed code over speculative concerns or hypothetical future problems.
- Use short sections and bullets.
- Make the result specific to reviewing this PR, not a generic SDRangel architecture document.
- Before reporting a defect involving an existing class, method, or API, inspect its implementation and relevant callers when necessary to establish its actual behavior. Follow relevant ownership, lifetime, locking, cleanup, synchronization, inheritance, virtual methods, signals, slots, and initialization paths outside the diff. Do not infer behavior or missing functionality from method names, types, or changed code alone.
- Review changed code in the context of the actual repository implementation, including code outside the PR when it defines the behavior of changed code. Use that code to determine whether the PR introduces a defect, but do not report unrelated pre-existing issues.
- Base statements on actual repository code, documentation, and established project practices.
- Do not invent requirements or impose generic software-engineering practices.
- Do not recommend or request automated tests, unit tests, regression tests, test harnesses, or new test infrastructure. SDRangel does not have an automated test framework, and the absence of one is not a defect or a review finding.
- Never add a generic “Test suggestions” section solely because a PR lacks automated tests.
- Omit sections that are not relevant to the PR.
- When code and documentation disagree, do not assume the code is wrong. Treat the discrepancy as a documentation issue unless there is independent evidence of a code defect.
