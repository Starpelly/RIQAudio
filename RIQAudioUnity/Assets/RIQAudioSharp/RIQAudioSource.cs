using UnityEngine;

namespace RIQAudioUnity.Unity
{
    // Test
    public class RIQAudioSource : MonoBehaviour
    {
        public string testFileLoc;
        Sound soundtest;

        private void Start()
        {
            RIQAudio.RiqInitAudioDevice();

            testFileLoc = Application.streamingAssetsPath + "/" + testFileLoc;

            soundtest = RIQAudio.RiqLoadSound(testFileLoc);
        }

        private void Update()
        {
            if (Input.GetMouseButtonDown(0))
            {
                RIQAudio.RiqPlaySound(soundtest);
            }
        }

        private void OnDestroy()
        {
            RIQAudio.RiqUnloadSound(soundtest);
            RIQAudio.RiqCloseAudioDevice();
        }
    }
}
