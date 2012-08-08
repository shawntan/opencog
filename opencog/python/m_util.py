##
# @file m_util.py
# @brief developing python library
# @author Dingjie.Wang
# @version 1.0
# @date 2012-08-04


import re
import inspect
from pprint import pprint
def dict_sub(d, text):
  """ Replace in 'text' non-overlapping occurences of REs whose patterns are keys
  in dictionary 'd' by corresponding values (which must be constant strings: may
  have named backreferences but not numeric ones). The keys must not contain
  anonymous matching-groups.
  Returns the new string.""" 
  try:
  # Create a regular expression  from the dictionary keys
      regex = re.compile("|".join("(%s)" % k for k in d))
      # Facilitate lookup from group number to value
      lookup = dict((i+1, v) for i, v in enumerate(d.itervalues()))

      # For each match, find which group matched and expand its value
      return regex.sub(lambda mo: mo.expand(lookup[mo.lastindex]), text)
  except Exception:
      return text

def format_log(offset, dsp_caller = True, *args):
    '''  '''
    caller = "" 
    if dsp_caller:
        stack = inspect.stack()
        caller = " -- %s %s" % (stack[1][2], stack[1][3])

    out =  ' ' * offset +  ' '.join(map(str, args)) + caller
    return out

class Logger(object):
    DEBUG = 0
    INFO = 1
    WARNING = 2
    ERROR = 3
    # colorful output
    BLUE = '\033[94m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    COLOR_END = '\033[0m'
    def __init__(self, f = 'opencog-python.log'):
        try:
            self._file = open(f,'w')
        except IOError:
            print " error: can't open logging file %s " % f
        self._filename = f
        self._levels = set()
        self.offset = 0
        #
        self.to_stdout = True
        self.to_file = True
        self.add_level(Logger.ERROR)
    
    def debug(self,msg, head = "" ):
        try:
            if self.to_file and Logger.DEBUG in self._levels:
                temp = "[DEBUG]" + head + ":" + str(msg) if head else "[DEBUG]" + str(msg)
                print >>self._file, temp
        except IOError:
            print  Logger.RED + " error: can't write logging file %s " % self._filename + Logger.COLOR_END

        if self.to_stdout and Logger.DEBUG in self._levels:
            temp = "[DEBUG]" + head + ":" + str(msg) if head else "[DEBUG]" + str(msg)
            print Logger.BLUE + temp + Logger.COLOR_END

    def info(self, msg, head = "" ):
        try:
            if self.to_file and Logger.INFO in self._levels:
                temp = "[INFO]" + head + ":" + str(msg) if head else "[INFO]" + str(msg)
                print >>self._file, temp
        except IOError:
            print  Logger.RED + " error: can't write logging file %s " % self._filename + Logger.COLOR_END

        if self.to_stdout and Logger.INFO in self._levels:
            temp = "[INFO]" + head + ":" + str(msg) if head else "[INFO]" + str(msg)
            print Logger.GREEN + temp + Logger.COLOR_END
            

    def warning(self,msg, head = "" ):
        try:
            if self.to_file and Logger.WARNING in self._levels:
                temp = "[WARNING]" + head + ":" + str(msg) if head else "[WARNING]" + str(msg)
                print >>self._file, temp
        except IOError:
            print  Logger.RED + " error: can't write logging file %s " % self._filename + Logger.COLOR_END


        if self.to_stdout and Logger.WARNING in self._levels:
            temp = "[WARNING]" + head + ":" + str(msg) if head else "[WARNING]" + str(msg)
            print Logger.YELLOW + temp + Logger.COLOR_END

    def error(self, msg, head = "" ):
        try:
            if self.to_file and Logger.ERROR in self._levels:
                temp = "[ERROR]" + head + ":" + str(msg) if head else "[ERROR]" + str(msg)
                print >>self._file, temp
        except IOError:
            print  Logger.RED + " error: can't write logging file %s " % self._filename + Logger.COLOR_END

        if self.to_stdout and Logger.ERROR in self._levels:
            temp = "[ERROR]" + head + ":" + str(msg) if head else "[ERROR]" + str(msg)
            print Logger.RED + temp + Logger.COLOR_END

    def pprint(self, obj):
        '''docstring for pprint()''' 
        try:
            if self.to_file:
                pprint(obj, self._file)
        except IOError:
            print  Logger.RED + " error: can't write logging file %s " % self._filename + Logger.COLOR_END
        if self.to_stdout:
            pprint(obj)

    def flush(self):
        self._file.flush()
    
    def use_stdout(self, use):
        self.to_stdout = use

    #def setLevel(self, level):
        #self._levels.append(level)
    def add_level(self, level):
        self._levels.add(level)
        '''docstring for add_level''' 
log = Logger()
#log.add_level(Logger.ERROR)
__all__ = ["log", "Logger", "replace_with_dict"]
