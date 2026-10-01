Review SDRangel pull requests using the repository and current PR as context.

Focus on finding real defects, regressions, compatibility problems, and relevant documentation inconsistencies in the changes being reviewed. Use repository-specific conventions and architecture only when they are relevant to the changed code. If everything looks OK, be as brief as possible.

Focus on the current PR, not on producing general SDRangel documentation.

Cover:

- **PR context**: What the PR changes, which subsystems it affects, and any repository-specific context needed to review it correctly.

- **Relevant architecture**: Describe only the SDRangel architecture relevant to the changed code. Use concrete concepts such as plugins, devices, channels, features, DSP components, GUI components, message queues, FIFOs, worker threads, or WebAPI where applicable. Do not force the code into generic patterns such as MVC, MVVM, or monorepo architecture.

- **Relevant code structure**: Identify the classes, interfaces, directories, message types, queues, build files, generated code, or other components that matter to the PR. Do not describe unrelated parts of the repository.

- **Relevant dependencies and tooling**: Mention Qt, CMake, external libraries, hardware APIs, generated code, CI, static analysis, sanitizers, or other tooling only when relevant to the PR.

- **Testing and validation**: Describe appropriate validation for the actual changes. Directed manual testing using SDRangel, relevant features, devices, or hardware is valid. Do not suggest automated checks such as compilation, CI, cppcheck, Coverity, OpenGrep, or sanitizers.
  Do not assume unit tests are required. SDRangel does not have an established general unit-testing practice. Prefer the project's existing validation methods, including directed manual testing where appropriate.

- **Relevant conventions and pitfalls**: Identify established SDRangel conventions and concrete risks relevant to the changed code, such as thread/lifetime races, message ownership, queue handling, startup/shutdown ordering, Qt ownership, DSP/device lifecycle, settings serialization, API compatibility, generated code, or CMake dependencies. Only include issues relevant to the PR.

- **Scope discipline**: Review the changes in the PR rather than looking for unrelated existing issues. Do not report pre-existing problems unless the PR introduces, exposes, or materially changes them.

- **Documentation consistency**: Use relevant repository documentation, developer notes, and wiki material as review context. Assume the current code is authoritative when documentation or wiki content disagrees with it. Do not flag correct code merely because it differs from stale documentation. Instead, point out the documentation discrepancy when the PR changes or exposes behavior that is documented incorrectly or incompletely. Do not require documentation changes for purely internal implementation changes.

- **Generated and third-party code**: Identify generated, vendored, or externally maintained code relevant to the PR. Do not apply normal SDRangel source-review expectations to such code unless the PR intentionally modifies or integrates it.

- **Out of scope**: Identify things the reviewer should not flag for this PR. Do not request unrelated refactoring, broad modernization, new abstractions, architectural redesign, or generic best-practice changes. Do not flag missing unit tests when they are not an established requirement for the affected code.

Rules:
- Output only the Markdown content, with no preamble or explanation.
- Be concise. Every line should earn its place.
- Prefer concrete, actionable findings tied to changed code over speculative concerns or hypothetical future problems.
- Use short sections and bullets.
- Make the result specific to reviewing this PR, not a generic SDRangel architecture document.
- Base statements on actual repository code, documentation, and established project practices.
- Do not invent requirements or impose generic software-engineering practices.
- Omit sections that are not relevant to the PR.
- When code and documentation disagree, do not assume the code is wrong. Treat the discrepancy as a documentation issue unless there is independent evidence of a code defect.
