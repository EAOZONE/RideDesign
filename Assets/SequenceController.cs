using System.Collections;
using UnityEngine;
using MQTTnet;
using MQTTnet.Client;
using System.Text;
using System.Threading.Tasks;

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

    // ================= MQTT =================
    private IMqttClient mqttClient;
    private string mqttBrokerIP = "10.160.121.143";
    private int mqttPort = 1883;

    async void Start()
    {
        await SetupMQTT();
        StartCoroutine(RunSequence());
    }

    private async Task SetupMQTT()
{
    var factory = new MqttFactory();
    mqttClient = factory.CreateMqttClient();

    var options = new MqttClientOptionsBuilder()
        .WithTcpServer("10.160.121.143", 1883)
        .WithCleanSession()
        .Build();

    await mqttClient.ConnectAsync(options);
    Debug.Log("MQTT Connected");
}

    private async Task PublishBeamState(string beamName, int state)
{
    if (mqttClient == null || !mqttClient.IsConnected)
        return;

    string payload = "{\"id\":\"" + beamName + "\",\"state\":" + state + "}";

    var message = new MqttApplicationMessageBuilder()
        .WithTopic("wayside/beam")
        .WithPayload(Encoding.UTF8.GetBytes(payload))
        .WithQualityOfServiceLevel(
            MQTTnet.Protocol.MqttQualityOfServiceLevel.AtLeastOnce
        )
        .Build();

    await mqttClient.PublishAsync(message);

    Debug.Log("MQTT Sent: " + payload);
}

    // =====================================================
    // MAIN SEQUENCE
    // =====================================================
    private IEnumerator RunSequence()
    {
        yield return MoveUntilBeam(trackA, switchBeamName);

        yield return new WaitForSeconds(delayBeforeSwitching);

        switchAnimator.SetTrigger(toggleTriggerName);

        yield return WaitForAnimationState("Diverge");

        yield return new WaitForSeconds(delayBeforeTrackB);

        yield return MoveUntilBeam(trackB, exitBeamName);

        yield return new WaitForSeconds(delayBeforeSwitching);

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
            yield return null;

        while (switchAnimator.GetCurrentAnimatorStateInfo(0).normalizedTime < 1.0f)
            yield return null;
    }

    // =====================================================
    // BEAM DETECTION
    // =====================================================
    private async void OnTriggerEnter(Collider other)
    {
        if (!other.CompareTag(beamTag))
            return;

    if (other.name == switchBeamName)
    {
        await PublishBeamState(other.name, 1);
    }
    else if (other.name == exitBeamName)
    {
        await PublishBeamState(switchBeamName, 0);
    }
    }
}