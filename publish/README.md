# Publishing the skills collection to package registries

The repository itself is the source of truth. This folder contains ready-to-publish
packages that install the skill collection into an agent's skills directory via a
single command — the "pip/npm" distribution path.

## What each package does

Both `trothbyte-skills` packages expose one command:

```bash
trothbyte-skills install [targetDir]
```

It clones the repository and copies every `skills/<domain>/<id>/SKILL.md` into
`targetDir` (default: `~/.claude/skills`).

## Publishing requires your accounts (one-time)

An AI agent cannot publish to npm or PyPI without your credentials. Everything below
is prepared; you only run the commands after logging in.

### npm (publish/npm)

```bash
cd publish/npm
npm view trothbyte-skills        # check the name is free
npm login                         # your npm account (2FA)
npm publish                       # publishes trothbyte-skills@0.1.0
```

### PyPI (publish/pypi)

```bash
cd publish/pypi
pip install build twine
python -m build
python -m twine upload dist/*     # your PyPI account (2FA)
```

After publishing, anyone can run:

```bash
npx trothbyte-skills install            # npm
pipx install trothbyte-skills           # PyPI
```

## Notes

- Version bump: change `version` in `package.json` / `pyproject.toml` and the
  `__version__` in `trothbyte_skills/__init__.py` for every release.
- The packages contain no skill content — they fetch it from GitHub at install time,
  so the published artifact never goes stale.
- Name availability should be checked before the first publish; if `trothbyte-skills`
  is taken, use `trothbyte-low-level-skills`.
