using System.Collections;
using UnityEngine;

public class SwitchTrackSequence : MonoBehaviour
{
    [Header("Cube")]
    public Transform cube;
    public float moveSpeed = 1.5f;
    public float arriveEpsilon = 0.01f;

    [Header("Waypoints")]
    public Transform[] trackA;   // up to the switch
    public Transform[] trackB;   // after the switch

    [Header("Switch Track (choose one approach)")]
    public Animator switchAnimator;     // Option A
    public string toggleTriggerName = "Toggle";

    // Option B (code-driven switch)
    public Transform switchRoot;
    public Transform switchStateStraight;
    public Transform switchStateDiverge;
    public float switchTime = 0.5f;

    public bool useAnimator = true;

    private void Start()
    {
        StartCoroutine(RunSequence());
    }

    private IEnumerator RunSequence()
    {
        // 1) Move cube through first track to the switch
        yield return MoveAlong(trackA);

        // 2) Switch track
        if (useAnimator && switchAnimator != null)
        {
            switchAnimator.SetTrigger(toggleTriggerName);

            // If you have an animation length, either:
            // - wait a fixed time that matches it, or
            // - use Animation Events to signal completion.
            yield return new WaitForSeconds(switchTime);
        }
        else
        {
            // Code-driven switch (lerp from straight to diverge)
            yield return LerpTransform(switchRoot, switchStateDiverge, switchTime);
        }

        // 3) Move cube through next track
        yield return MoveAlong(trackB);
    }

    private IEnumerator MoveAlong(Transform[] waypoints)
    {
        foreach (var wp in waypoints)
        {
            yield return MoveTo(cube, wp.position);
        }
    }

    private IEnumerator MoveTo(Transform obj, Vector3 targetPos)
    {
        // Move with constant speed, frame-rate independent
        while ((obj.position - targetPos).sqrMagnitude > arriveEpsilon * arriveEpsilon)
        {
            obj.position = Vector3.MoveTowards(obj.position, targetPos, moveSpeed * Time.deltaTime);
            yield return null;
        }
        obj.position = targetPos;
    }

    private IEnumerator LerpTransform(Transform root, Transform target, float duration)
    {
        Vector3 startPos = root.position;
        Quaternion startRot = root.rotation;

        Vector3 endPos = target.position;
        Quaternion endRot = target.rotation;

        float t = 0f;
        while (t < 1f)
        {
            t += Time.deltaTime / Mathf.Max(0.0001f, duration);
            root.position = Vector3.Lerp(startPos, endPos, t);
            root.rotation = Quaternion.Slerp(startRot, endRot, t);
            yield return null;
        }

        root.position = endPos;
        root.rotation = endRot;
    }
}