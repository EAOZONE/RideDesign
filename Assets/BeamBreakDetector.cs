using UnityEngine;

public class BeamBreakDetector : MonoBehaviour
{
    void OnTriggerEnter(Collider other)
    {
        if (other.CompareTag("BeamBreak"))
        {
            Debug.Log("Beam Broken at " + other.name);
        }
    }
}