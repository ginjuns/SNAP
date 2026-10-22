package com.example.snapapp.ui

import android.Manifest
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Matrix
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageCapture
import androidx.camera.core.ImageCaptureException
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.LocalLifecycleOwner

private const val FACE_COUNT = 5

@Composable
fun FaceCaptureDialog(
    onDismiss: () -> Unit,
    onCaptured: (List<Bitmap>) -> Unit
) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current

    /* 카메라 촬영 및 사진 목록 */
    val imageCapture = remember {
        ImageCapture.Builder().build()
    }

    val photos = remember {
        mutableStateListOf<Bitmap>()
    }

    var capturing by remember {
        mutableStateOf(false)
    }

    var message by remember {
        mutableStateOf("정면을 보고 촬영해 주세요")
    }

    /* 카메라 권한 확인 */
    var hasPermission by remember {
        mutableStateOf(
            ContextCompat.checkSelfPermission(
                context,
                Manifest.permission.CAMERA
            ) == PackageManager.PERMISSION_GRANTED
        )
    }

    val done = photos.size >= FACE_COUNT

    /* 카메라 권한 요청 */
    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { granted ->
        hasPermission = granted

        if (!granted) {
            message = "카메라 권한을 허용해야 얼굴 정보를 등록할 수 있습니다"
        }
    }

    LaunchedEffect(Unit) {
        if (!hasPermission) {
            permissionLauncher.launch(Manifest.permission.CAMERA)
        }
    }

    /* 화면 종료 시 카메라 리소스 해제 */
    DisposableEffect(Unit) {
        onDispose {
            val future = ProcessCameraProvider.getInstance(context)

            future.addListener(
                {
                    future.get().unbindAll()
                },
                ContextCompat.getMainExecutor(context)
            )
        }
    }

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(
            usePlatformDefaultWidth = false
        )
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black)
                .systemBarsPadding(),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            /* 화면 제목 */
            Text(
                text = "얼굴 정보 등록",
                color = Color.White,
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(16.dp)
            )

            /* 카메라 미리보기 또는 촬영 완료 사진 */
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth(),
                contentAlignment = Alignment.Center
            ) {
                if (done) {
                    PhotoRow(
                        photos = photos,
                        modifier = Modifier.padding(horizontal = 16.dp)
                    )
                } else if (hasPermission) {
                    AndroidView(
                        factory = { ctx ->
                            val previewView = PreviewView(ctx)
                            val future =
                                ProcessCameraProvider.getInstance(ctx)

                            future.addListener(
                                {
                                    val provider = future.get()
                                    val preview = Preview.Builder().build()

                                    preview.setSurfaceProvider(
                                        previewView.surfaceProvider
                                    )

                                    provider.unbindAll()

                                    provider.bindToLifecycle(
                                        lifecycleOwner,
                                        CameraSelector.DEFAULT_FRONT_CAMERA,
                                        preview,
                                        imageCapture
                                    )
                                },
                                ContextCompat.getMainExecutor(ctx)
                            )

                            previewView
                        },
                        modifier = Modifier.fillMaxSize()
                    )
                }
            }

            /* 촬영한 사진 미리보기 */
            if (!done && photos.isNotEmpty()) {
                PhotoRow(
                    photos = photos,
                    modifier = Modifier.padding(
                        horizontal = 16.dp,
                        vertical = 8.dp
                    )
                )
            }

            /* 안내 메시지 */
            Text(
                text = message,
                color = Color.White,
                fontSize = 14.sp,
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(16.dp)
            )

            /* 하단 버튼 영역 */
            Row(
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(
                        horizontal = 24.dp,
                        vertical = 16.dp
                    )
            ) {
                if (!done) {
                    /* 촬영 취소 */
                    OutlinedButton(
                        onClick = onDismiss,
                        modifier = Modifier
                            .weight(1f)
                            .height(48.dp)
                    ) {
                        Text(
                            text = "취소",
                            color = Color.White
                        )
                    }

                    /* 사진 촬영 */
                    Button(
                        onClick = {
                            capturing = true

                            imageCapture.takePicture(
                                ContextCompat.getMainExecutor(context),
                                object : ImageCapture.OnImageCapturedCallback() {

                                    override fun onCaptureSuccess(
                                        image: ImageProxy
                                    ) {
                                        try {
                                            /* 이미지 회전 정보 적용 */
                                            val matrix = Matrix()

                                            matrix.postRotate(
                                                image.imageInfo.rotationDegrees
                                                    .toFloat()
                                            )

                                            val bitmap = image.toBitmap()

                                            /* 회전된 사진을 목록에 추가 */
                                            photos.add(
                                                Bitmap.createBitmap(
                                                    bitmap,
                                                    0,
                                                    0,
                                                    bitmap.width,
                                                    bitmap.height,
                                                    matrix,
                                                    true
                                                )
                                            )
                                        } finally {
                                            image.close()
                                            capturing = false
                                        }

                                        /* 촬영 진행 상태 안내 */
                                        message = if (
                                            photos.size >= FACE_COUNT
                                        ) {
                                            "사진을 확인하고 등록해 주세요"
                                        } else {
                                            "얼굴 각도를 조금씩 바꿔서 촬영해 주세요"
                                        }
                                    }

                                    override fun onError(
                                        exception: ImageCaptureException
                                    ) {
                                        capturing = false
                                        message =
                                            "촬영에 실패했어요. 다시 시도해 주세요"
                                    }
                                }
                            )
                        },
                        enabled = hasPermission && !capturing,
                        modifier = Modifier
                            .weight(1f)
                            .height(48.dp)
                    ) {
                        Text("촬영 (${photos.size}/$FACE_COUNT)")
                    }
                } else {
                    /* 사진 전체 재촬영 */
                    OutlinedButton(
                        onClick = {
                            photos.clear()
                            message = "정면을 보고 촬영해 주세요"
                        },
                        modifier = Modifier
                            .weight(1f)
                            .height(48.dp)
                    ) {
                        Text(
                            text = "재촬영",
                            color = Color.White
                        )
                    }

                    /* 촬영한 사진 등록 */
                    Button(
                        onClick = {
                            onCaptured(photos.toList())
                        },
                        modifier = Modifier
                            .weight(1f)
                            .height(48.dp)
                    ) {
                        Text("등록")
                    }
                }
            }
        }
    }
}

@Composable
private fun PhotoRow(
    photos: List<Bitmap>,
    modifier: Modifier
) {
    Row(
        horizontalArrangement = Arrangement.spacedBy(6.dp),
        modifier = modifier.fillMaxWidth()
    ) {
        /* 사진을 최대 5개까지 나란히 표시 */
        repeat(FACE_COUNT) { i ->
            Box(
                modifier = Modifier
                    .weight(1f)
                    .aspectRatio(3f / 4f)
                    .clip(RoundedCornerShape(8.dp))
                    .background(Color.DarkGray)
            ) {
                photos.getOrNull(i)?.let { bitmap ->
                    Image(
                        bitmap = bitmap.asImageBitmap(),
                        contentDescription = null,
                        contentScale = ContentScale.Crop,
                        modifier = Modifier.fillMaxSize()
                    )
                }
            }
        }
    }
}