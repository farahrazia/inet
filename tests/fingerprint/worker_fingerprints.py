import subprocess
import os
import shutil
import sys
import re
import time
import logging

import pymongo
import gridfs
import flock

from fingerprints import SimulationResult

MODEL_DIR = "/tmp/model/"


inetRoot = "/opt/inet"
inetLibCache = "/host-cache/inetlib"
sep = ";" if sys.platform == 'win32' else ':'
nedPath = inetRoot + "/src" + sep + inetRoot + "/examples" + sep + inetRoot + "/showcases" + sep + inetRoot + "/tutorials" + sep + inetRoot + "/tests/networks"
inetLib = inetRoot + "/src/INET"
inetLibFile = inetRoot + "/src/libINET.so"
cpuTimeLimit = "30000s"
executable = "opp_run_release"
logFile = "test.out"
extraOppRunArgs = ""
exitCode = 0

mongoHost = "mng" # discovered using swarm internal dns

INET_HASH_FILE = "/opt/current_inet_hash"


logger = logging.getLogger("rq.worker")


def runProgram(command, workingdir):
    logger.info("running command: " + str(command))

    process = subprocess.Popen(command, shell=True, cwd=workingdir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    out = process.communicate()[0]
    out = re.sub("\r", "", out.decode('utf-8'))

    logger.info("done, exitcode: " + str(process.returncode))

    return (process.returncode, out)


def checkoutGit(gitHash):
    (exitCode, output) = runProgram("git fetch", inetRoot)
    if exitCode != 0:
        raise Exception("Failed to fetch git repo:\n" + output)

    (exitCode, output) = runProgram("git reset --hard " + gitHash, inetRoot)
    if exitCode != 0:
        raise Exception("Failed to fetch git repo:\n" + output)

# this is the first kind of job
def buildInet(gitHash):
    checkoutGit(gitHash)

    client = pymongo.MongoClient(mongoHost)
    gfs = gridfs.GridFS(client.opp)

    if gfs.exists(gitHash):
        return "INET build " + gitHash + " already exists, not rebuilding."

    (exitCode, output) = runProgram("make makefiles", inetRoot)
    if exitCode != 0:
        raise Exception("Failed to make makefiles:\n" + output)

    (exitCode, output) = runProgram("make -j $(distcc -j) MODE=release", inetRoot)
    if exitCode != 0:
        raise Exception("Failed to build INET:\n" + output)

    f = open(inetLibFile, "rb")
    gfs.put(f, _id = gitHash)

    return "INET " + gitHash + " built and stored."


def replaceInetLib(gitHash):
    directory = inetLibCache + "/" + gitHash

    ensure_dir(directory)

    logger.info("replacing inet lib with the one built from commit " + gitHash)

    try:
        with open(directory + "/libINET.so", "xb") as f:
            
            # i THINK there could be a race condition if another worker locks the
            # file with LOCK_SH (in the other branch) while we are right here

            logger.info("we have just created the file, so we need to download it")
            with flock.Flock(f, flock.LOCK_EX):
                logger.info("write lock acquired")
                client = pymongo.MongoClient(mongoHost)
                gfs = gridfs.GridFS(client.opp)
                logger.info("connected, downloading")
                f.write(gfs.get(gitHash).read())
                logger.info("download done")

    except FileExistsError:
        logger.info("the file was created by someone else, waiting for it to be downloaded")
        with open(directory + "/libINET.so", "wb") as f:
            with flock.Flock(f, flock.LOCK_SH):
                logger.info("file download finished")

    shutil.copy(directory + "/libINET.so", inetLibFile)
    logger.info("file copied to the right place")

def switchInetToCommit(gitHash):

    logger.info("switching inet to commit " + gitHash)

    currentHash = ""
    try:
        with open(INET_HASH_FILE, "rt") as f:
            currentHash = str(f.read()).strip()
    except:
        pass # XXX

    logger.info("we are currently on commit: " + currentHash)

    # only switching if not already on the same hash
    if gitHash != currentHash:
        logger.info("actually doing it")
        checkoutGit(gitHash)

        replaceInetLib(gitHash)

        try:
            with open(INET_HASH_FILE, "wt") as f:
                f.write(gitHash)
        except:
            pass # XXX

        logger.info("done")
    else:
        logger.info("not switching, already on it")


def runSimulation(title, command, workingdir, resultdir):
    global logFile
    ensure_dir(workingdir + "/results")

    logger.info("running simulation")
    # run the program and log the output
    t0 = time.time()

    (exitcode, out) = runProgram(command, workingdir)

    elapsedTime = time.time() - t0

    logger.info("sim terminated, elapsedTime: " + str(elapsedTime))

    FILE = open(logFile, "a")
    FILE.write("------------------------------------------------------\n"
             + "Running: " + title + "\n\n"
             + "$ cd " + workingdir + "\n"
             + "$ " + command + "\n\n"
             + out.strip() + "\n\n"
             + "Exit code: " + str(exitcode) + "\n"
             + "Elapsed time:  " + str(round(elapsedTime,2)) + "s\n\n")
    FILE.close()

    FILE = open(resultdir + "/test.out", "w")
    FILE.write("------------------------------------------------------\n"
             + "Running: " + title + "\n\n"
             + "$ cd " + workingdir + "\n"
             + "$ " + command + "\n\n"
             + out.strip() + "\n\n"
             + "Exit code: " + str(exitcode) + "\n"
             + "Elapsed time:  " + str(round(elapsedTime,2)) + "s\n\n")
    FILE.close()

    result = SimulationResult(command, workingdir, exitcode, elapsedTime=elapsedTime)
    result.out = out
    # process error messages
    errorLines = re.findall("<!>.*", out, re.M)
    errorMsg = ""
    for err in errorLines:
        err = err.strip()
        if re.search("Fingerprint", err):
            if re.search("successfully", err):
                result.isFingerprintOK = True
            else:
                m = re.search("(computed|calculated): ([-a-zA-Z0-9]+(/[a-z0]+)?)", err)
                if m:
                    result.isFingerprintOK = False
                    result.computedFingerprint = m.group(2)
                else:
                    raise Exception("Cannot parse fingerprint-related error message: " + err)
        else:
            errorMsg += "\n" + err
            if re.search("CPU time limit reached", err):
                result.cpuTimeLimitReached = True
            m = re.search("at event #([0-9]+), t=([0-9]*(\\.[0-9]+)?)", err)
            if m:
                result.numEvents = int(m.group(1))
                result.simulatedTime = float(m.group(2))

    result.errormsg = errorMsg.strip()
    return result

def _iif(cond,t,f):
    return t if cond else f

def ensure_dir(f):
    if not os.path.exists(f):
        os.makedirs(f)




# this is the second kind of job
def runTest(gitHash, title, csvFile, wd, args, simtimelimit, fingerprint, repeat): # storeFingerprintCallback, storeExitcodeCallback):
    # CPU time limit is a safety guard: fingerprint checks shouldn't take forever
    
    testBeginTime = time.time()
    logger.info("test started: " + str(testBeginTime))

    global inetRoot, cpuTimeLimit, executable

    switchInetToCommit(gitHash)

    # run the simulation
    workingdir = _iif(wd.startswith('/'), inetRoot + "/" + wd, wd)
    wdname = '' + wd + ' ' + args
    wdname = re.sub('/', '_', wdname)
    wdname = re.sub('[\\W]+', '_', wdname)
    resultdir = workingdir + "/results/" + csvFile + "/" + wdname
    if not os.path.exists(resultdir):
        try:
            os.makedirs(resultdir)
        except OSError:
            pass
    command = executable + " -n " + nedPath + " -l " + inetLib + " -u Cmdenv " + args + \
        _iif(simtimelimit != "", " --sim-time-limit=" + simtimelimit, "") + \
        " \"--fingerprint=" + fingerprint + "\" --cpu-time-limit=" + cpuTimeLimit + \
        " --vector-recording=false --scalar-recording=true" + \
        " --result-dir=" + resultdir + \
        extraOppRunArgs
    

    logger.info("COMMAND: " + command)

    anyFingerprintBad = False
    computedFingerprints = set()
    for rep in range(repeat):

        result = runSimulation(title, command, workingdir, resultdir)

        # process the result
        # note: fingerprint mismatch is technically NOT an error in 4.2 or before! (exitcode==0)

        if result.exitcode != 0:
            return "runtime error:" + result.errormsg + result.out
        elif result.cpuTimeLimitReached:
            return "cpu time limit exceeded"
        elif result.simulatedTime == 0 and simtimelimit != '0s':
            return "zero time simulated"
        elif result.isFingerprintOK is None:
            return "other error"
        elif result.isFingerprintOK == False:
            computedFingerprints.add(result.computedFingerprint)
            anyFingerprintBad = True
        else:
            # fingerprint OK:
            computedFingerprints.add(fingerprint)

    testEndTime = time.time()
    logger.info("test ended: " + str(testEndTime))
    logger.info("test duration: " + str(testEndTime - testBeginTime))

    if anyFingerprintBad:
        return "some fingerprint mismatch; actual " + " '" + ",".join(computedFingerprints) +"'" # + result.out

    return "OK, took " + str(result.elapsedTime)
