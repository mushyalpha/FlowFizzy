# SpeakDojo Engineering Standards (Rigor Edition)

Version: 1.0  
Owner: SpeakDojo engineering team  
Goal: Ship fast with AI-assisted TypeScript while maintaining reliability and quality standards stricter than the embedded `project_checklist`.

---

## 1. Engineering Constitution (Non-Negotiable Rules)

These rules are hard gates. If any rule fails, the change does not merge.

1. Type safety is mandatory.
   - TypeScript `strict` mode enabled.
   - `noUncheckedIndexedAccess`, `exactOptionalPropertyTypes`, `noImplicitOverride`, `noFallthroughCasesInSwitch` enabled.
   - No `any` in app code.
   - Exception: adapter boundary files only, with comment `// justified-any: <reason>`.
   - Why: type holes become production bugs when speed increases.

2. Runtime validation at all trust boundaries.
   - Every inbound external payload (HTTP request body, query params, webhooks, queue messages, local storage) validated with schema (`zod` or equivalent).
   - Response payloads to clients also validated for critical endpoints.
   - Why: TypeScript protects compile-time assumptions, not runtime input.

3. Mandatory CI quality gates on every PR.
   - `lint`, `typecheck`, `unit`, `integration`, `e2e-smoke`, `security-scan`, `dependency-audit` must pass.
   - No direct pushes to `main`.
   - Minimum 1 review for low-risk PR, 2 reviews for high-risk PR.
   - Why: rigor must be automated to scale with fast iteration.

4. Production bug policy.
   - Every confirmed bug requires a regression test before fix merge.
   - Incident postmortem required for Sev-1/Sev-2 incidents.
   - Why: each bug is a process failure signal, not only a code failure.

5. Explicit performance and reliability budgets.
   - Critical API p95 latency budget defined and enforced.
   - Error-rate and uptime SLOs defined and tracked.
   - Why: if not measured, quality silently decays.

6. AI-generated code policy.
   - AI output is draft code, never trusted code.
   - Human reviewer must verify correctness, security, edge cases, and tests.
   - Why: velocity without verification increases hidden defect rate.

---

## 2. Quality Bar (Stricter Than Embedded Checklist)

The embedded checklist is excellent for hardware and RT constraints. SpeakDojo goes further by enforcing operational and product-level rigor:

1. Embedded style rigor:
   - deterministic behavior expectations
   - strict interfaces
   - explicit failure handling

2. Added web/service rigor:
   - strong schema validation at runtime
   - service-level objectives (SLOs) and error budgets
   - security gates and dependency governance
   - incident response and postmortem process
   - progressive rollout with feature flags and rollback plans

Practical definition: a feature is not "done" until it is safe to operate in production, not just correct on a laptop.

---

## 3. Definition of Done (Feature-Level Gate)

A feature can be merged only if all boxes are true:

1. Spec written (short but explicit).
   - Problem statement
   - Inputs/outputs
   - Edge cases
   - Failure modes

2. Types and schemas complete.
   - No `any` introduced.
   - Request/response schemas implemented.

3. Tests complete.
   - Unit tests for domain logic.
   - Integration tests for DB/network boundaries.
   - At least one e2e scenario for user-visible behavior.
   - Regression test for any bug fixed during feature work.

4. Observability complete.
   - Structured logs for key events.
   - Metrics added for key outcomes and failure modes.
   - Trace spans for critical path where applicable.

5. Operational safety complete.
   - Feature flag or staged rollout defined for risky changes.
   - Rollback procedure documented in PR.
   - Migration plan and rollback for schema/data changes.

6. Review quality complete.
   - PR description includes "what", "why", "risk", "test evidence".
   - Reviewer checklist fully answered.

---

## 4. Required Code Standards (TypeScript + Architecture)

1. Layered architecture.
   - Domain layer: pure logic, no framework imports.
   - Application layer: orchestrates use-cases.
   - Infrastructure layer: DB/API adapters.
   - UI layer: rendering and interaction only.
   - Why: testability and change isolation.

2. Dependency rule.
   - Inner layers never depend on outer layers.
   - Why: prevents framework lock-in and tangled coupling.

3. Domain logic purity.
   - Business rules in pure functions where possible.
   - Side-effects isolated.
   - Why: easier testing and lower bug surface.

4. Error handling.
   - No silent catch blocks.
   - Expected failures returned as typed results.
   - Unexpected failures logged with correlation IDs.
   - Why: silent failures create ghost bugs.

5. API contracts.
   - API versioning strategy required for breaking changes.
   - Contract tests for public API endpoints.
   - Why: preserves backward compatibility.

6. Naming and readability.
   - Names must explain intent, not implementation detail.
   - Functions should be small and single-purpose.
   - Why: readability is long-term speed.

---

## 5. Testing Standards (Minimums)

1. Coverage thresholds.
   - Global: 85% line, 80% branch.
   - Critical modules (payments, auth, entitlements, streak engine): 95% line, 90% branch.
   - Why: high-risk code needs higher confidence.

2. Mutation testing.
   - Run mutation tests on critical domain modules.
   - Mutation score target: >= 70%.
   - Why: catches weak tests that only exercise happy paths.

3. Test pyramid enforcement.
   - Unit tests: many, fast.
   - Integration tests: moderate.
   - E2E tests: focused, critical journeys only.
   - Why: speed and confidence balance.

4. Deterministic test execution.
   - No test depends on wall-clock time or random seed without control.
   - Use fake timers for time logic.
   - Why: flaky tests destroy trust in CI.

5. Contract and migration tests.
   - DB migrations must have forward and rollback checks.
   - External API contracts pinned and tested.
   - Why: most production failures happen at boundaries.

---

## 6. Reliability and SRE Standards

1. SLOs (initial recommendation).
   - API availability: 99.9% monthly.
   - Critical endpoint p95 latency: < 300 ms.
   - Background job success rate: >= 99.5%.
   - Why: SLOs turn quality goals into objective decisions.

2. Error budgets.
   - If error budget burn > 2x normal rate, freeze feature work for stabilization.
   - Why: prevents shipping velocity from outrunning system health.

3. Runbooks.
   - Required for auth outage, payment outage, data corruption scenario.
   - Why: execution speed under pressure depends on pre-written playbooks.

4. Incident management.
   - Sev-1 response time target: <= 15 minutes.
   - Postmortem template required with corrective action owners and deadlines.
   - Why: rigor includes response quality, not only prevention.

---

## 7. Security Standards

1. Secure defaults.
   - Least privilege for DB and cloud credentials.
   - Secrets never in source control.
   - Why: credential leaks are high-impact failures.

2. Dependency governance.
   - Automated vulnerability scan per PR.
   - Critical CVEs block merge unless explicit security waiver.
   - Why: supply chain risk is continuous.

3. Auth and session controls.
   - Enforce secure cookies, CSRF protection, and token expiry/rotation where relevant.
   - Why: auth weaknesses are common breach vectors.

4. Input and output hardening.
   - Validate and sanitize all user input.
   - Escape output where required by render context.
   - Why: prevents injection classes of vulnerabilities.

---

## 8. Data Integrity and Migration Standards

1. Migration discipline.
   - Every schema migration has backward compatibility strategy.
   - Destructive migrations must be two-phase.
   - Why: protects uptime and rollback capability.

2. Idempotency for critical writes.
   - Payment, enrollment, entitlement, and progress updates must be idempotent.
   - Why: retries are normal in distributed systems.

3. Auditability.
   - Critical user-impacting changes (entitlements, billing state) must be audit logged.
   - Why: enables debugging, trust, and compliance readiness.

---

## 9. Frontend Standards (If SpeakDojo Has Web UI)

1. Accessibility.
   - WCAG 2.2 AA target for key pages.
   - Keyboard navigation and focus states mandatory.
   - Why: accessibility is quality, not decoration.

2. Performance.
   - Core Web Vitals budget set per route.
   - Bundle size budgets with CI alerts.
   - Why: slow UI harms learning outcomes and retention.

3. State management rigor.
   - Server state and UI state separated.
   - Avoid hidden mutable shared state.
   - Why: state bugs are subtle and expensive.

---

## 10. AI-Assisted Development Rules

1. Prompt discipline.
   - Prompt includes constraints: architecture layer, tests required, no `any`, failure paths.
   - Why: quality of generated code follows clarity of constraints.

2. Verification loop.
   - Generate -> inspect -> test -> harden -> re-test.
   - Why: one-pass generation is not engineering.

3. Human accountability.
   - Reviewer signs off that generated code matches team standards.
   - Why: accountability cannot be outsourced.

---

## 11. PR Template (Required Sections)

Every PR must include:

1. What changed.
2. Why it changed.
3. Risk assessment (low/medium/high) and why.
4. Test evidence (commands and outputs summary).
5. Observability updates (logs/metrics/traces).
6. Rollback plan.
7. Screenshots or recordings for UI changes.

PRs missing any section are not reviewed.

---

## 12. SpeakDojo Domain-Specific Standards

1. Streak and schedule correctness.
   - Timezone-safe logic with tests for DST and day boundaries.
   - Why: learning apps are time-sensitive by design.

2. Lesson progress integrity.
   - Progress writes idempotent and monotonic where expected.
   - Why: user trust collapses if progress disappears or rewinds.

3. Payment and entitlement safety.
   - Payment success must not grant duplicate entitlements.
   - Payment retry must not double-charge.
   - Why: billing defects are high-severity trust failures.

4. AI feedback safeguards.
   - Prompt/response logging for moderation and debugging.
   - Safety checks for generated content.
   - Why: AI quality and safety must be observable.

---

## 13. Weekly Engineering Rituals

1. Weekly stability review (30 min).
   - Top errors, SLO burn, flaky tests, slow endpoints.

2. Weekly architecture review (30 min).
   - Identify coupling creep and shortcut debt.

3. Weekly bug taxonomy review.
   - Classify root causes and install process safeguards.

Why: rigor decays without cadence.

---

## 14. Metrics Dashboard (Minimum)

Track and review weekly:

1. Change failure rate.
2. Mean time to recovery (MTTR).
3. CI pass rate and flaky test rate.
4. Escaped defects per release.
5. p95 latency and error rate for critical routes.
6. Dependency vulnerability backlog.

Why: high standards require measurable feedback loops.

---

## 15. Reading List for Deep Rigor (Books + Why)

1. Code Complete (Steve McConnell)
   - Why: practical engineering discipline and construction best practices.

2. Clean Architecture (Robert C. Martin)
   - Why: long-term maintainability and dependency direction.

3. Designing Data-Intensive Applications (Martin Kleppmann)
   - Why: correctness, consistency, replication, idempotency, and system tradeoffs.

4. Accelerate (Nicole Forsgren, Jez Humble, Gene Kim)
   - Why: evidence-based practices for speed with reliability.

5. Site Reliability Engineering (Google SRE book)
   - Why: SLOs, error budgets, operations discipline.

6. The DevOps Handbook (Gene Kim et al.)
   - Why: flow, feedback loops, and production-safe delivery.

7. Practical Monitoring (Mike Julian)
   - Why: observability that supports debugging and confidence.

8. Web Application Security (Andrew Hoffman)
   - Why: practical threat-driven security approach for web products.

9. Refactoring (Martin Fowler)
   - Why: safely improving design as the codebase evolves.

10. Working Effectively with Legacy Code (Michael Feathers)
   - Why: safe change strategies when speed meets existing complexity.

---

## 16. Keep-This-In-Mind Coding Heuristics

When coding fast, pause and ask:

1. What assumption could break in production?
2. What happens on retries, duplicates, and timeouts?
3. What invariant must always remain true?
4. How will I detect failure quickly if this goes wrong?
5. Can I roll this back safely within minutes?

If you cannot answer these clearly, the feature is not production-ready yet.

---

## 17. Adoption Plan (First 2 Weeks)

Week 1:

1. Turn on strict TS flags and lint rules.
2. Add required CI gates.
3. Add PR template and enforce branch protections.
4. Define initial SLOs for top 3 critical endpoints.

Week 2:

1. Add runtime schemas for all external API boundaries.
2. Add e2e smoke tests for top 3 user journeys.
3. Add incident runbooks for auth and payment failures.
4. Instrument logs/metrics for critical paths.

This gives immediate rigor without blocking product velocity for months.
