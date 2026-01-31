# =====================================================================================
# SWE‑1.5 RULESET — FILE CREATION & MODIFICATION POLICY
# =====================================================================================

# TRIGGER MODE
trigger: manual
# The agent must never run automatically. It must only execute actions when the user
# explicitly triggers a run. No autonomous scanning, planning, or file operations.

# =====================================================================================
# FILE CREATION RULES
# =====================================================================================

# 1. DO NOT CREATE FILES THAT ALREADY EXIST
# -------------------------------------------------------------------------------------
# If a file already exists anywhere in the workspace, the agent must NOT create a new
# file with the same name, same path, or same purpose. Instead, the agent must modify
# the existing file in place.
#
# This rule is absolute. No exceptions. No renaming. No duplication. No shadow copies.
# If the user requests creation of a file that already exists, the agent must interpret
# the request as a modification request and update the existing file instead.

# 2. ONLY CREATE NEW FILES WHEN:
# -------------------------------------------------------------------------------------
# - The file does not exist anywhere in the workspace.
# - The user explicitly requests creation of a new file.
# - The file is required for the requested feature or fix.
#
# The agent must verify non‑existence before creating anything.

# 3. NEVER CREATE PLACEHOLDER FILES
# -------------------------------------------------------------------------------------
# The agent must not generate empty files, stub files, placeholder modules, or
# speculative files. Every created file must contain complete, functional, production‑
# ready content.

# =====================================================================================
# FILE MODIFICATION RULES
# =====================================================================================

# 4. MODIFY EXISTING FILES IN PLACE
# -------------------------------------------------------------------------------------
# When a file exists, the agent must:
# - Open the file
# - Apply minimal, targeted modifications
# - Preserve structure, formatting, and style
# - Avoid rewriting unrelated sections
#
# The agent must NOT:
# - Rewrite entire files unless explicitly instructed
# - Reformat the entire file
# - Change indentation style
# - Change naming conventions
# - Introduce comments unless explicitly requested

# 5. PRESERVE USER STYLE AND ARCHITECTURE
# -------------------------------------------------------------------------------------
# The agent must maintain:
# - Existing architecture
# - Existing module boundaries
# - Existing naming conventions
# - Existing file layout
# - Existing logic flow
#
# The agent must not redesign or restructure unless explicitly instructed.

# =====================================================================================
# SAFETY & CONSISTENCY RULES
# =====================================================================================

# 6. NO MULTI‑FILE REWRITES
# -------------------------------------------------------------------------------------
# The agent must not rewrite multiple files unless the user explicitly requests a
# multi‑file change. Each run should be minimal and precise.

# 7. NO AUTOMATIC DEPENDENCY CREATION
# -------------------------------------------------------------------------------------
# The agent must not create new modules, headers, libraries, or dependencies unless the
# user explicitly requests them.

# 8. NO SPECULATION
# -------------------------------------------------------------------------------------
# The agent must not guess or assume missing files, missing modules, or missing
# architecture. It must only operate on what exists.

# =====================================================================================
# EXECUTION RULES
# =====================================================================================

# 9. MANUAL TRIGGER ONLY
# -------------------------------------------------------------------------------------
# The agent must not run automatically. It must wait for the user to explicitly trigger
# execution. No background runs. No auto‑fixing. No auto‑planning.

# 10. OUTPUT MUST BE COMPLETE AND READY
# -------------------------------------------------------------------------------------
# Every output must be:
# - Fully complete
# - Ready to paste
# - Buildable
# - Free of placeholders
# - Free of comments unless requested
#
# The agent must never output partial code or incomplete modules.

# =====================================================================================
# END OF RULESET
# =====================================================================================
