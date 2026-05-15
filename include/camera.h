#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

// camera movement options
enum class CameraMovement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};

// default camera values
inline constexpr float YAW			= -90.0f;
inline constexpr float PITCH		= 0.0f;
inline constexpr float SPEED		= 7.5f;
inline constexpr float SENSITIVITY	= 0.1f;
inline constexpr float ZOOM			= 90.0f;

// cameracontroller class
class Camera
{
public:
	static constexpr float MIN_NEARPLANE = 0.1f;
	static constexpr float MIN_MOVESPEED = 5.0f;
	static constexpr float MAX_MOVESPEED = 10.0f;
	static constexpr float MIN_FARPLANE = 2000.0f;
	static constexpr float MAX_FARPLANE = 4000.0f;
public:
	// constructor with vectors
	Camera(int width, int height, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH);

	// constructor with scalar values
	Camera(int width, int height, float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

	~Camera();

	// setters
	void setLastX(float lastX);
	void setLastY(float lastY);
	void setFirstMouse(bool isFirstMouse);
	void setAccelerationMultiplier(float multiplier);

	float getAccelerationMultiplier() const;

	// returns the view matrix calculated using Euler angles and LookAt matrix
	glm::mat4 getViewMatrix() const;

	// processes input received from any keyboard-like input system.
	// accepts input parameter in the form of camera defined ENUM
	void processKeyboard(CameraMovement direction, float deltaTime);

	// processes input received from a mouse input system. expects the offset value
	// in both the x and y direction
	void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

	// processes input received from a mouse scroll-wheel event. only requires
	// input on the vertical wheel-axis
	void processMouseScroll(float yoffset);

	// invert pitch
	void invertPitch();

	void onResize(int w, int h);

	// mouse handlers
	void handleMousePosition(float xpos, float ypos, bool constrainPitch = true);
	void handleMouseScroll(float yoffset);

	void setEnabled(bool enabled);
	bool isEnabled() const;

	const glm::mat4 getProjectionMatrix(float aspectRatio) const;
	const glm::mat4 getProjectionMatrixVk(float aspectRatio) const;

	const glm::mat4 getJitterProjectionMatrixVk(float aspectRatio, const glm::vec2& jitter) const;

	const glm::vec3& getCameraPosition() const;
	void setCameraPosition(const glm::vec3& pos);

	glm::vec3 getCameraDirection() const;
	glm::vec3 getCameraUp() const;
	glm::vec3 getCameraFront() const;


	float getNearPlane() const;
	float getFarPlane() const;
	void setFarPlane(float fp);

	float getMovementSpeed() const;
	void setMovementSpeed(float speed);

private:
	// width of window
	int width_{};
	// height of window
	int height_{};
	// camera x,y position center
	float lastX_{};
	float lastY_{};
	// first mouse movement
	bool isFirstMouse_ = true;

	bool isEnabled_ = true;

	// camera attributes
	glm::vec3 position_{};
	glm::vec3 front_{};
	glm::vec3 up_{};
	glm::vec3 right_{};
	glm::vec3 worldUp_{};

	float nearPlane_{ MIN_NEARPLANE };
	float farPlane_{ MIN_FARPLANE };

	// euler angles
	float yaw_{};
	float pitch_{};

	// camera options
	float movementSpeed_{};
	float mouseSensitivity_{};
	float zoom_{};
	float accelerationMultiplier_{ 1.0f };
private:
	// calculates the front vector from the Camera's (updated) Euler angles
	void updateCameraVectors();
};

#endif