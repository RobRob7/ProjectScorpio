#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

//--- PUBLIC ---//
// constructor with vectors
Camera::Camera(int width, int height, glm::vec3 position, glm::vec3 up, float yaw, float pitch)
	: width_(width), height_(height), front_(glm::vec3(0.0f, 0.0f, -1.0f)), movementSpeed_(SPEED), mouseSensitivity_(SENSITIVITY), zoom_(ZOOM)
{
	lastX_ = width_ / 2.0f;
	lastY_ = height_ / 2.0f;
	position_ = position;
	worldUp_ = up;
	yaw_ = yaw;
	pitch_ = pitch;
	updateCameraVectors();
} // end constructor

// constructor with scalar values
Camera::Camera(int width, int height, float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch)
	: width_(width), height_(height), front_(glm::vec3(0.0f, 0.0f, -1.0f)), movementSpeed_(SPEED), mouseSensitivity_(SENSITIVITY), zoom_(ZOOM)
{
	lastX_ = width_ / 2.0f;
	lastY_ = height_ / 2.0f;
	position_ = glm::vec3(posX, posY, posZ);
	worldUp_ = glm::vec3(upX, upY, upZ);
	yaw_ = yaw;
	pitch_ = pitch;
	updateCameraVectors();
} // end constructor

Camera::~Camera() = default;

// setters
void Camera::setLastX(float lastX)
{
	lastX_ = lastX;
} // end of setLastX()

void Camera::setLastY(float lastY)
{
	lastY_ = lastY;
} // end of setLastY()

void Camera::setFirstMouse(bool isFirstMouse)
{
	isFirstMouse_ = isFirstMouse;
} // end of setFirstMouse()

void Camera::setAccelerationMultiplier(float multiplier)
{
	accelerationMultiplier_ = multiplier;
} // end of setAccelerationMultiplier() 

float Camera::getAccelerationMultiplier() const
{
	return accelerationMultiplier_;
} // end of getAccelerationMultiplier()

// returns the view matrix calculated using Euler angles and LookAt matrix
glm::mat4 Camera::getViewMatrix() const
{
	return glm::lookAt(position_, position_ + front_, up_);
} // end of getViewMatrix()

// processes input received from any keyboard-like input system.
// accepts input parameter in the form of camera defined ENUM
void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
	if (isEnabled_)
	{
		float velocity = movementSpeed_ * deltaTime * accelerationMultiplier_;
		if (direction == CameraMovement::FORWARD)
			position_ += front_ * velocity;
		if (direction == CameraMovement::BACKWARD)
			position_ -= front_ * velocity;
		if (direction == CameraMovement::LEFT)
			position_ -= right_ * velocity;
		if (direction == CameraMovement::RIGHT)
			position_ += right_ * velocity;
	}
} // end of processKeyboard()

// processes input received from a mouse input system. expects the offset value
// in both the x and y direction
void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
	// only process if camera is enabled
	if (isEnabled_)
	{
		xoffset *= mouseSensitivity_;
		yoffset *= mouseSensitivity_;

		yaw_ += xoffset;
		pitch_ += yoffset;

		// check pitch within bounds
		if (constrainPitch)
		{
			if (pitch_ > 89.0f)
				pitch_ = 89.0f;
			if (pitch_ < -89.0f)
				pitch_ = -89.0f;
		}

		// update m_front, m_right, and m_up vectors using updated Euler angles
		updateCameraVectors();
	}
} // end of processMouseMovement()

// processes input received from a mouse scroll-wheel event. only requires
// input on the vertical wheel-axis
void Camera::processMouseScroll(float yoffset)
{
	// only process if camera is enabled
	if (!isEnabled_) return;

	zoom_ -= (float)yoffset;
	if (zoom_ < 1.0f)
		zoom_ = 1.0f;
	if (zoom_ > ZOOM)
		zoom_ = ZOOM;
} // end of processMouseScroll()

// invert pitch
void Camera::invertPitch()
{
	pitch_ = -pitch_;

	// prior enable state
	bool wasEnabled = isEnabled_;
	isEnabled_ = true;
	updateCameraVectors();

	// resore prior enable state
	isEnabled_ = wasEnabled;
} // end of invertPitch()

void Camera::onResize(int w, int h)
{
	width_ = w;
	height_ = h;
} // end of onResize()

// mouse handlers
void Camera::handleMousePosition(float xpos, float ypos, bool constrainPitch)
{
	if (!isEnabled_) return;

	if (isFirstMouse_)
	{
		lastX_ = xpos;
		lastY_ = ypos;
		isFirstMouse_ = false;
	}

	float xoffset = xpos - lastX_;
	float yoffset = lastY_ - ypos;

	lastX_ = xpos;
	lastY_ = ypos;

	processMouseMovement(xoffset, yoffset, constrainPitch);
} // end of handleMousePosition()

void Camera::handleMouseScroll(float yoffset)
{
	processMouseScroll(yoffset);
} // end of handleMouseScroll()

void Camera::setEnabled(bool enabled)
{
	// from disabled to enabled, treat next
	// mouse event as first one
	if (enabled && !isEnabled_)
	{
		isFirstMouse_ = true;
	}
	isEnabled_ = enabled;
} // end of setEnabled()

bool Camera::isEnabled() const
{
	return isEnabled_;
} // end of isEnabled()

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const
{
	return glm::perspective(glm::radians(zoom_), aspectRatio, nearPlane_, farPlane_);
} // end of getProjectionMatrix()

glm::mat4 Camera::getProjectionMatrixVk(float aspectRatio) const
{
	return glm::perspective(glm::radians(zoom_), aspectRatio, nearPlane_, farPlane_);
} // end of getProjectionMatrixVk()

glm::vec3 Camera::getCameraPosition() const
{
	return position_;
} // end of getCameraPosition()

glm::vec3& Camera::getCameraPosition()
{
	return position_;
} // end of getCameraPosition()

void Camera::setCameraPosition(const glm::vec3& pos)
{
	position_ = pos;
} // end of setCameraPosition()

glm::vec3 Camera::getCameraDirection() const
{
	return front_;
} // end of getCameraDirection()

glm::vec3 Camera::getCameraUp() const
{
	return up_;
} // end of getCameraUp()

glm::vec3 Camera::getCameraFront() const
{
	return front_;
} // end of getCameraFront()

float Camera::getNearPlane() const
{
	return nearPlane_;
} // end of getNearPlane()

float Camera::getFarPlane() const
{
	return farPlane_;
} // end of getFarPlane()

void Camera::setFarPlane(float fp)
{
	farPlane_ = std::clamp(fp, MIN_FARPLANE, MAX_FARPLANE);
} // end of setFarPlane()

float Camera::getMovementSpeed() const
{
	return movementSpeed_;
} // end of getMovementSpeed()

void Camera::setMovementSpeed(float speed)
{
	movementSpeed_ = std::clamp(speed, MIN_MOVESPEED, MAX_MOVESPEED);
} // end of setMovementSpeed()


//--- PRIVATE ---//
// calculates the front vector from the Camera's (updated) Euler angles
void Camera::updateCameraVectors()
{
	// only process if camera is enabled
	if (isEnabled_)
	{
		// calculate the new m_front vector
		glm::vec3 front{};
		front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
		front.y = sin(glm::radians(pitch_));
		front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
		front_ = glm::normalize(front);

		// re-calculate m_right and m_up vector
		right_ = glm::normalize(glm::cross(front_, worldUp_));
		up_ = glm::normalize(glm::cross(right_, front_));
	}
} // end of updateCameraVectors()
