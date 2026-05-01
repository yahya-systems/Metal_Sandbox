#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>

class Camera {
  glm::vec3 position;
  glm::vec3 forward;
  glm::vec3 up;
  glm::vec3 right;
  float pitch, yaw;
  float moveSpeed;
  float moveSensitivity;

public:
  glm::mat4 *matricesBuffer;
  uint32_t screenWidth, screenHeight;
  Camera(glm::mat4 *buffer, float sensitivity, float speed, uint32_t width,
         uint32_t height)
      : matricesBuffer(buffer), moveSensitivity(sensitivity), moveSpeed(speed),
        screenWidth(width), screenHeight(height), position(glm::vec3(0.0f)),
        forward(glm::vec3(0.0f, 0.0f, 1.0f)), up(glm::vec3(0.0f, 1.0f, 0.0f)),
        right(glm::vec3(1.0f, 0.0f, 0.0f)) {}

  void update(float pitchOffset, float yawOffset, int forwardMul, int rightMul,
              int upMul) {
    pitch += pitchOffset * moveSensitivity;
    yaw += yawOffset * moveSensitivity;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    glm::quat q =
        glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.0f));
    forward = glm::normalize(q * glm::vec3(0.0f, 0.0f, -1.0f));
    right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    up = glm::normalize(glm::cross(forward, right));

    position += forward * moveSpeed * (float)forwardMul +
                right * moveSpeed * (float)rightMul +
                up * moveSpeed * (float)upMul;

    if (!matricesBuffer)
      return;

    matricesBuffer[0] =
        glm::lookAt(position, position + forward, glm::vec3(0.0f, 1.0f, 0.0f));

    matricesBuffer[1] = glm::perspective(
        glm::radians(60.0f), (float)screenWidth / screenHeight, 0.2f, 10000.0f);
  }
};
