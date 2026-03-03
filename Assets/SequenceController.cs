using System.Collections;
using UnityEngine;

public class SwitchTrackSequence : MonoBehaviour
{
    [Header("Cube")]
    public Rigidbody cubeRB;
    public float moveSpeed = 1.5f;
    public float arriveEpsilon = 0.01f;

    [Header("Waypoints")]
    public Transform[] trackA;
    public Transform[] trackB;

    [Header("Beam Breakers")]
    public string beamTag = "BeamBreak";
    public string switchBeamName = "SwitchBeam";
    public string exitBeamName = "ExitBeam";

    private string lastBeamHit = "";

    [Header("Delays")]
    public float delayBeforeSwitching = 1.0f;
    public float delayBeforeTrackB = 1.0f;

    [Header("Switch Track")]
    public Animator switchAnimator;
    public string toggleTriggerName = "Toggle";

    private void Start()
    {
        StartCoroutine(RunSequence());
    }

    // =====================================================
    // MAIN SEQUENCE
    // =====================================================
    private IEnumerator RunSequence()
{
    // Move along Track A until SwitchBeam
    yield return MoveUntilBeam(trackA, switchBeamName);

    yield return new WaitForSeconds(delayBeforeSwitching);

    // Toggle switch to Diverge
    switchAnimator.SetTrigger(toggleTriggerName);

    // Wait until animation finishes
    yield return WaitForAnimationState("Diverge");

    // ADD WAIT HERE
    yield return new WaitForSeconds(delayBeforeTrackB);

    // Move along Track B until ExitBeam
    yield return MoveUntilBeam(trackB, exitBeamName);

    // ADD WAIT HERE
    yield return new WaitForSeconds(delayBeforeSwitching);

    // Toggle switch back to Straight
    switchAnimator.SetTrigger(toggleTriggerName);

    yield return WaitForAnimationState("Straight");

    Debug.Log("Sequence Complete");
}

    // =====================================================
    // MOVE UNTIL SPECIFIC BEAM
    // =====================================================
    private IEnumerator MoveUntilBeam(Transform[] waypoints, string requiredBeam)
    {
        lastBeamHit = "";

        int index = 0;

        while (lastBeamHit != requiredBeam && index < waypoints.Length)
        {
            Vector3 targetPos = waypoints[index].position;

            while ((cubeRB.position - targetPos).sqrMagnitude > arriveEpsilon * arriveEpsilon)
            {
                if (lastBeamHit == requiredBeam)
                    yield break;

                Vector3 newPos = Vector3.MoveTowards(
                    cubeRB.position,
                    targetPos,
                    moveSpeed * Time.fixedDeltaTime
                );

                cubeRB.MovePosition(newPos);
                yield return new WaitForFixedUpdate();
            }

            index++;
        }

        Debug.Log("Arrived at beam: " + requiredBeam);
    }

    // =====================================================
    // WAIT FOR ANIMATOR STATE
    // =====================================================
    private IEnumerator WaitForAnimationState(string stateName)
    {
        while (!switchAnimator.GetCurrentAnimatorStateInfo(0).IsName(stateName))
        {
            yield return null;
        }

        while (switchAnimator.GetCurrentAnimatorStateInfo(0).normalizedTime < 1.0f)
        {
            yield return null;
        }
    }

    // =====================================================
    // BEAM DETECTION
    // =====================================================
    private void OnTriggerEnter(Collider other)
    {
        if (!other.CompareTag(beamTag))
            return;

        lastBeamHit = other.name;
        Debug.Log("Beam Broken: " + other.name);
    }
}