# RUN: %PYTHON %s 2>&1 | FileCheck %s

import gc, sys
from mlir.ir import *
from mlir.passmanager import *

# Log everything to stderr and flush so that we have a unified stream to match
# errors/info emitted by MLIR to stderr.
def log(*args):
  print(*args, file=sys.stderr)
  sys.stderr.flush()

def run(f):
  log("\nTEST:", f.__name__)
  f()
  gc.collect()
  assert Context._get_live_count() == 0

# Verify capsule interop.
# CHECK-LABEL: TEST: testCapsule
def testCapsule():
  with Context():
    pm = PassManager()
    pm_capsule = pm._CAPIPtr
    assert '"mlir.passmanager.PassManager._CAPIPtr"' in repr(pm_capsule)
    pm._testing_release()
    pm1 = PassManager._CAPICreate(pm_capsule)
    assert pm1 is not None  # And does not crash.
run(testCapsule)


# Verify successful round-trip.
# CHECK-LABEL: TEST: testParseSuccess
def testParseSuccess():
  with Context():
    # A first import is expected to fail because the pass isn't registered
    # until we import mlir.transforms
    try:
      pm = PassManager.parse("module(func(print-op-stats))")
      # TODO: this error should be propagate to Python but the C API does not help right now.
      # CHECK: error: 'print-op-stats' does not refer to a registered pass or pass pipeline
    except ValueError as e:
      # CHECK: ValueError exception: invalid pass pipeline 'module(func(print-op-stats))'.
      log("ValueError exception:", e)
    else:
      log("Exception not produced")

    # This will register the pass and round-trip should be possible now.
    import mlir.transforms
    pm = PassManager.parse("module(func(print-op-stats))")
    # CHECK: Roundtrip: module(func(print-op-stats))
    log("Roundtrip: ", pm)
run(testParseSuccess)

# Verify failure on unregistered pass.
# CHECK-LABEL: TEST: testParseFail
def testParseFail():
  with Context():
    try:
      pm = PassManager.parse("unknown-pass")
    except ValueError as e:
      # CHECK: ValueError exception: invalid pass pipeline 'unknown-pass'.
      log("ValueError exception:", e)
    else:
      log("Exception not produced")
run(testParseFail)


# Verify failure on incorrect level of nesting.
# CHECK-LABEL: TEST: testInvalidNesting
def testInvalidNesting():
  with Context():
    try:
      pm = PassManager.parse("func(print-op-graph)")
    except ValueError as e:
      # CHECK: Can't add pass 'PrintOp' restricted to 'module' on a PassManager intended to run on 'func', did you intend to nest?
      # CHECK: ValueError exception: invalid pass pipeline 'func(print-op-graph)'.
      log("ValueError exception:", e)
    else:
      log("Exception not produced")
run(testInvalidNesting)


# Verify that a pass manager can execute on IR
# CHECK-LABEL: TEST: testRun
def testRunPipeline():
  with Context():
    pm = PassManager.parse("print-op-stats")
    module = Module.parse(r"""func @successfulParse() { return }""")
    pm.run(module)
# CHECK: Operations encountered:
# CHECK: func              , 1
# CHECK: module            , 1
# CHECK: module_terminator , 1
# CHECK: std.return        , 1
run(testRunPipeline)
