.PHONY: validate eval security test token

validate:
	python tools/validate.py

eval:
	python tools/eval_runner.py

security:
	semgrep --config=p/cwe-top-25 skills/

test: validate eval security

token:
	python -c "import glob,os,subprocess,sys; dirs=sorted({os.path.dirname(p) for p in glob.glob('skills/**/SKILL.md', recursive=True)}); sys.exit(subprocess.call([sys.executable,'tools/tokens/token_measure.py','--check','2000']+dirs))"
