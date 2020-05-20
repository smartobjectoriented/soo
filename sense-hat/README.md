In python-sense-emu:
python setup.py bdist_wheel

and move the dist/<package>.whl in sense-hat/ directory

python -m pip install <package>.whl

To be executed in python-sense-hat:

# PYTHONPATH=../out/lib/python2.7/site-packages/ python setup.py install --prefix=../out

