using UnityEngine;

namespace RIQAudio.Unity
{
    public class RIQAudioSource : MonoBehaviour
    {
        public string testFileLoc;
        Sound soundtest;

        private void Start()
        {
            testFileLoc = Application.streamingAssetsPath + "/mudstep_atomicbeats_old.wav";

            RIQDLL.RiqInitAudioDevice();

            // Should return true if RiqInitAudioDevice worked.
            Debug.Log(RIQDLL.IsRiqReady());

            soundtest = RIQDLL.RiqLoadSound(Application.streamingAssetsPath + "/hit.ogg");
        }

        private void Update()
        {
            if (Input.GetMouseButtonDown(0))
            {
                RIQDLL.RiqPlaySound(soundtest);
            }
        }

        private void OnDestroy()
        {
            RIQDLL.RiqUnloadSound(soundtest);
            RIQDLL.RiqCloseAudioDevice();
        }
    }
}
