# Bambuddy Fork Development Instructions

This repository is a maintained fork of the upstream Bambuddy project. Custom
features are expected and may intentionally differ from upstream.

## Branch roles

- `upstream/main` is the official Bambuddy source of truth.
- `origin/main` must remain an exact mirror of `upstream/main`. Never add custom
  commits, merge feature branches, or deploy fork-only changes from `main`.
- `origin/custom/main` is the fork's production integration branch. It contains
  the latest integrated upstream code plus all approved fork-specific changes.
- Create fork-specific feature and fix branches from `origin/custom/main`, then
  merge them back into `custom/main`.
- Create changes intended for an upstream pull request directly from
  `upstream/main`, with no fork-specific commits in their history.

## Worktree workflow

- Always create a dedicated git worktree before starting branch work. Never
  edit in the shared clone or reuse another task's worktree.
- Fetch `origin` and `upstream` before selecting a base.
- Use one branch and one worktree per feature or fix.
- Do not alter or remove unrelated changes in another worktree.

## Integrating upstream

- Update `origin/main` from `upstream/main` without adding merge commits or
  fork-specific changes.
- Integrate upstream into `custom/main` with a normal merge. Do not rebase or
  force-push the long-lived `custom/main` branch.
- Resolve conflicts in a dedicated integration worktree and validate both the
  upstream behavior and affected custom features before pushing.
- Keep custom changes in focused commits so they are easy to review, repair, or
  remove when upstream later implements equivalent behavior.
- When upstream supersedes a customization, remove the redundant fork code
  during the upstream integration rather than maintaining two implementations.

## Testing and review

- Run the smallest existing tests, lint checks, and builds that cover a change.
- Add regression coverage for behavior changes.
- Treat a clean upstream test failure as a baseline issue only after reproducing
  it independently of the custom branch.
- Pull requests for fork-only work should target `custom/main`, not `main`.

## Deployment

- Deploy nuc1 only from `custom/main` or from a temporary branch whose complete
  history is based on the current `custom/main`.
- Never deploy a branch based only on `upstream/main`; doing so would silently
  remove the fork's existing features.
- Before replacing the Bambuddy container, preserve a rollback image and make a
  consistent SQLite backup.
- Reuse the existing Compose project name `bambuddy` so deployment retains the
  `bambuddy_data` and `bambuddy_logs` volumes.
- Check active printer and queue state before restart. Container replacement
  must not send stop commands or intentionally interrupt active prints.
- After deployment, confirm container health, restart recovery, queue integrity,
  and the deployed image revision.
