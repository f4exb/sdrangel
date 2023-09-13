// Request permission.authorization to use camera and microphone
// From: https://developer.apple.com/documentation/bundleresources/information_property_list/protected_resources/requesting_authorization_for_media_capture_on_macos?language=objc

#include <AVFoundation/AVFoundation.h>

// Returns:
//   1 - if permission granted,
//   0 - if pending,
//  -1 - if not granted.
int authCameraAndMic()
{
    // Request permission to access the camera and microphone.
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo])
    {
    case AVAuthorizationStatusAuthorized:
        // The user has previously granted access to the camera.
        return 1;
    case AVAuthorizationStatusNotDetermined:
        {
            // The app hasn't yet asked the user for camera access.
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
                if (granted) {
                }
            }];
            return 0;
        }
    case AVAuthorizationStatusDenied:
        // The user has previously denied access.
        return -1;
    case AVAuthorizationStatusRestricted:
        // The user can't grant access due to restrictions.
        return -1;
    }
}

