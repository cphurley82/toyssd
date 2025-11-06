"""Invoke entrypoint that re-exports the project task collection.

Rationale:
- Keeps all task logic out of the repository root while still allowing the
  conventional ``invoke <task>`` entrypoint to work from the project root.
- Avoids circular imports if ``tools`` grows helper modules.
"""

from tools.invoke_tasks import ns  # noqa: F401

