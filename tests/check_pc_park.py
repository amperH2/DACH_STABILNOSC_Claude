import importlib.util
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('park',ROOT/'tools/park_then_close.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Fake:
 def __init__(self,parked=False,error=False):self.parked=parked;self.error=error;self.calls=[]
 def call(self,member,method='GET',**kw):
  self.calls.append((member,method))
  if member in ('connected','canpark'):return True
  if member=='park':
   if self.error:raise m.AlpacaError('park failed')
   self.parked=True;return None
  if member=='atpark':return self.parked
  if member=='shutterstatus':return 1
  return None
class Tests(unittest.TestCase):
 def test_park_before_close(self):
  t=Fake();r=Fake();m.park_then_close(t,r);self.assertIn(('park','PUT'),t.calls);self.assertEqual(r.calls,[('closeshutter','PUT'),('shutterstatus','GET')])
 def test_already_parked(self):
  t=Fake(True);r=Fake();m.park_then_close(t,r);self.assertNotIn(('park','PUT'),t.calls)
 def test_failed_park_never_closes(self):
  r=Fake()
  with self.assertRaises(m.AlpacaError):m.park_then_close(Fake(error=True),r)
  self.assertEqual(r.calls,[])
 def test_timeout_never_closes(self):
  class Stuck(Fake):
   def call(self,member,method='GET',**kw):
    if member=='park':return None
    return super().call(member,method,**kw)
  now=[0];r=Fake()
  with self.assertRaises(m.AlpacaError):m.park_then_close(Stuck(),r,2,lambda:now[0],lambda s:now.__setitem__(0,now[0]+s))
  self.assertEqual(r.calls,[])
 def test_invalid_url(self):
  for base in ['file:///etc/passwd','http://user:pass@host','http://host/?token=secret']:
   with self.assertRaises(m.AlpacaError):m.Alpaca(base,'telescope')
if __name__=='__main__':unittest.main()
