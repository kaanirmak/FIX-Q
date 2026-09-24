# Security Policy

Finora is security-critical software used to protect financial exchanges, interbank clearing networks, and high-frequency trading pipelines. We take vulnerability reports with the highest level of urgency.

---

## Supported Versions

Only the latest minor release receives security updates and vulnerability patches.

| Version | Supported          |
|:--------|:-------------------|
| 1.4.x   | :white_check_mark: |
| < 1.4.0 | :x:                |

---

## Reporting a Vulnerability

If you discover or suspect a security vulnerability in Finora:

1. **DO NOT create a public GitHub issue.**
2. Send an encrypted email to **security@finora.io** (or open a private GitHub Security Advisory at `https://github.com/finora-pqc/finora/security/advisories/new`).
3. Include the following details in your report:
   - A clear description of the potential vulnerability.
   - Exact steps or proof-of-concept (PoC) code to reproduce the issue.
   - Assessment of potential impact (e.g. confidentiality loss, denial-of-service, replay attack).
   - Any proposed mitigations or code fixes.

---

## Response Timeline

- **Initial Acknowledgment:** Within **24 hours**.
- **Triage & Reproduction:** Within **48 hours**.
- **Fix & Advisory Release:** Within **7 business days** (or coordinated disclosure timeline agreed with the reporter).

We appreciate responsible disclosure and will credit security researchers in our release advisories.
