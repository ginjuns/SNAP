package com.example.snapapp.ui.admin

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.Base64
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.ShoppingCart
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.example.snapapp.ServerClient
import com.example.snapapp.toBase64
import com.example.snapapp.ui.LoadingDialog
import com.example.snapapp.ui.theme.SnapGreen
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.json.JSONObject

private const val SHELVES = 4

private data class Product(
    val id: Int,
    val shelf: Int,
    val name: String,
    val price: Int,
    val qty: Int,
    val image: Bitmap? = null
)

/* 서버가 보낸 Base64 사진을 이미지로 변환 */
private fun String.toBitmap(): Bitmap? {
    if (isEmpty()) return null

    val bytes = Base64.decode(this, Base64.DEFAULT)
    return BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
}

@Composable
fun ProductScreen() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val products = remember { mutableStateListOf<Product>() }

    /* 로딩 상태 */
    var loading by remember { mutableStateOf(false) }

    /* 안내 및 오류 메시지 */
    var message by remember { mutableStateOf<String?>(null) }

    /* 상품 등록 창 표시 여부 */
    var showAdd by remember { mutableStateOf(false) }

    /* 수정할 상품 */
    var editTarget by remember { mutableStateOf<Product?>(null) }

    /* 가격 수정 여부 */
    var editPrice by remember { mutableStateOf(false) }

    /* 삭제할 상품 */
    var deleteTarget by remember { mutableStateOf<Product?>(null) }

    /* 진열대를 변경할 상품 */
    var shelfTarget by remember { mutableStateOf<Product?>(null) }

    /* 사진을 변경할 상품 */
    var imageTarget by remember { mutableStateOf<Product?>(null) }

    /* 서버에 요청 보내기: 성공하면 응답, 실패하면 null */
    suspend fun call(body: JSONObject): JSONObject? {
        loading = true

        try {
            val res = ServerClient.request(body)

            if (res.optBoolean("ok")) {
                return res
            }

            message = res.optString("error", "요청에 실패했습니다.")
        } catch (e: Exception) {
            message = "서버에 연결할 수 없습니다."
        } finally {
            loading = false
        }

        return null
    }

    /* 서버에서 상품 목록 다시 받아오기 */
    suspend fun load() {
        val res = call(JSONObject().put("cmd", "PRODUCTS")) ?: return
        val list = res.getJSONArray("data")

        products.clear()

        for (i in 0 until list.length()) {
            val o = list.getJSONObject(i)

            products.add(
                Product(
                    id = o.optInt("id"),
                    shelf = o.optInt("shelf"),
                    name = o.optString("name"),
                    price = o.optInt("price"),
                    qty = o.optInt("stock"),
                    image = o.optString("image").toBitmap()
                )
            )
        }
    }

    /* 상품 한 개 수정 후 목록 새로고침 */
    fun edit(product: Product, key: String, value: Any) {
        scope.launch {
            val body = JSONObject()
                .put("cmd", "UPDATE_PRODUCT")
                .put("productId", product.id)
                .put(key, value)

            if (call(body) != null) {
                load()
            }
        }
    }

    LaunchedEffect(Unit) {
        load()
    }

    /* 사진 선택 */
    val photoPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.PickVisualMedia()
    ) { uri ->
        val target = imageTarget
        imageTarget = null

        if (uri != null && target != null) {
            val bitmap = context.contentResolver
                .openInputStream(uri)
                ?.use { BitmapFactory.decodeStream(it) }

            if (bitmap != null) {
                edit(target, "image", bitmap.toBase64())
            }
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .statusBarsPadding()
                .padding(horizontal = 24.dp)
        ) {
            Text(
                "상품 관리",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 16.dp, bottom = 16.dp)
            )

            if (products.isEmpty() && !loading) { // 등록된 상품이 없는 경우
                Text(
                    "등록된 상품이 없습니다.",
                    color = Color.Gray,
                    textAlign = TextAlign.Center,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 48.dp)
                )
            }

            /* 상품 목록 */
            LazyColumn {
                items(
                    products.sortedBy {
                        if (it.shelf in 1..SHELVES) it.shelf else 99
                    },
                    key = { it.id }
                ) { product ->
                    ProductItem(
                        product = product,
                        onNameClick = {
                            editPrice = false
                            editTarget = product
                        },
                        onPriceClick = {
                            editPrice = true
                            editTarget = product
                        },
                        onShelfClick = {
                            shelfTarget = product
                        },
                        onImageClick = {
                            imageTarget = product

                            photoPicker.launch(
                                PickVisualMediaRequest(
                                    ActivityResultContracts.PickVisualMedia.ImageOnly
                                )
                            )
                        },
                        onQtyChange = { q ->
                            val index = products.indexOfFirst {
                                it.id == product.id
                            }

                            if (index >= 0) {
                                products[index] = product.copy(qty = q)
                            }
                        },
                        onQtySend = { q ->
                            scope.launch {
                                try {
                                    val res = ServerClient.request(
                                        JSONObject()
                                            .put("cmd", "UPDATE_PRODUCT")
                                            .put("productId", product.id)
                                            .put("stock", q)
                                    )

                                    if (!res.optBoolean("ok")) {
                                        message = res.optString(
                                            "error",
                                            "수량 변경에 실패했습니다."
                                        )
                                    }
                                } catch (e: Exception) {
                                    message = "서버에 연결할 수 없습니다."
                                }
                            }
                        },
                        onDelete = {
                            deleteTarget = product
                        }
                    )
                }
            }
        }

        /* 상품 등록 버튼 */
        FloatingActionButton(
            onClick = {
                showAdd = true
            },
            containerColor = SnapGreen,
            contentColor = Color.White,
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .padding(24.dp)
        ) {
            Icon(
                Icons.Filled.Add,
                contentDescription = "상품 등록"
            )
        }
    }

    /* 상품명, 가격 수정 */
    editTarget?.let { target ->
        var text by remember(target) {
            mutableStateOf(
                if (editPrice) target.price.toString() else target.name
            )
        }

        AlertDialog(
            onDismissRequest = {
                editTarget = null
            },
            containerColor = Color.White,
            title = {
                Text(if (editPrice) "가격 수정" else "상품명 수정")
            },
            text = {
                OutlinedTextField(
                    value = text,
                    onValueChange = {
                        text = it
                    },
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(
                        keyboardType = if (editPrice) {
                            KeyboardType.Number
                        } else {
                            KeyboardType.Text
                        }
                    )
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        val price = text.toIntOrNull()

                        if (editPrice && price != null) {
                            editTarget = null
                            edit(target, "price", price)
                        } else if (!editPrice && text.isNotBlank()) {
                            editTarget = null
                            edit(target, "name", text)
                        }
                    }
                ) {
                    Text("저장")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        editTarget = null
                    }
                ) {
                    Text("취소")
                }
            }
        )
    }

    /* 진열대 번호 변경 */
    shelfTarget?.let { target ->
        val used = products
            .filter { it.id != target.id }
            .map { it.shelf }

        AlertDialog(
            onDismissRequest = {
                shelfTarget = null
            },
            containerColor = Color.White,
            title = {
                Text("진열대 변경")
            },
            text = {
                Column {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        (1..SHELVES).forEach { n ->
                            val selected = target.shelf == n

                            OutlinedButton(
                                onClick = {
                                    shelfTarget = null
                                    edit(target, "shelf", if (selected) 0 else n)
                                },
                                enabled = n !in used,
                                colors = ButtonDefaults.outlinedButtonColors(
                                    containerColor = if (selected) {
                                        SnapGreen
                                    } else {
                                        Color.Transparent
                                    },
                                    contentColor = if (selected) {
                                        Color.White
                                    } else {
                                        SnapGreen
                                    }
                                ),
                                contentPadding = PaddingValues(0.dp),
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("${n}번")
                            }
                        }
                    }

                    Text(
                        "선택된 번호를 다시 누르면 진열대 미지정",
                        fontSize = 13.sp,
                        color = Color.Gray,
                        modifier = Modifier.padding(top = 12.dp)
                    )
                }
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        shelfTarget = null
                    }
                ) {
                    Text("취소")
                }
            }
        )
    }

    /* 상품 삭제 */
    deleteTarget?.let { target ->
        AlertDialog(
            onDismissRequest = {
                deleteTarget = null
            },
            containerColor = Color.White,
            text = {
                Text(
                    "${target.name}을(를) 삭제하시겠습니까?",
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        deleteTarget = null

                        scope.launch {
                            val body = JSONObject()
                                .put("cmd", "DELETE_PRODUCT")
                                .put("productId", target.id)

                            if (call(body) != null) {
                                load()
                            }
                        }
                    }
                ) {
                    Text("삭제")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        deleteTarget = null
                    }
                ) {
                    Text("취소")
                }
            }
        )
    }

    /* 상품 등록 */
    if (showAdd) {
        ProductAddDialog(
            usedShelves = products.map { it.shelf },
            onDismiss = {
                showAdd = false
            },
            onAdd = { name, price, qty, shelf, image ->
                scope.launch {
                    val body = JSONObject()
                        .put("cmd", "ADD_PRODUCT")
                        .put("name", name)
                        .put("price", price)
                        .put("stock", qty)
                        .put("shelf", shelf ?: 0)
                        .put("image", image?.toBase64() ?: "")

                    if (call(body) != null) {
                        showAdd = false
                        message = "등록되었습니다."
                        load()
                    }
                }
            }
        )
    }

    if (loading) { // 로딩 창 표시
        LoadingDialog()
    }

    message?.let { // 안내, 오류 메시지 표시
        AlertDialog(
            onDismissRequest = {
                message = null
            },
            containerColor = Color.White,
            text = {
                Text(
                    it,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        message = null
                    }
                ) {
                    Text("확인")
                }
            }
        )
    }
}

@Composable
private fun ProductItem(
    product: Product,
    onNameClick: () -> Unit,
    onPriceClick: () -> Unit,
    onShelfClick: () -> Unit,
    onImageClick: () -> Unit,
    onQtyChange: (Int) -> Unit,
    onQtySend: (Int) -> Unit,
    onDelete: () -> Unit
) {
    var sentQty by remember {
        mutableIntStateOf(product.qty)
    }

    LaunchedEffect(product.qty) { // 수량을 바꾸고 나서 1초 뒤 최종 수량만 서버로 전송
        if (product.qty == sentQty) {
            return@LaunchedEffect
        }

        delay(1000)
        onQtySend(product.qty)
        sentQty = product.qty
    }

    Column(
        modifier = Modifier.padding(vertical = 12.dp)
    ) {
        /* 진열대 번호 */
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.clickable {
                onShelfClick()
            }
        ) {
            Text(
                if (product.shelf in 1..SHELVES) {
                    "${product.shelf}번 진열대"
                } else {
                    "진열대 미지정"
                },
                fontSize = 14.sp,
                color = Color.Gray
            )

            Icon(
                Icons.Filled.Edit,
                contentDescription = "진열대 변경",
                tint = Color.Gray,
                modifier = Modifier
                    .padding(start = 4.dp)
                    .size(14.dp)
            )
        }

        /* 상품 사진, 정보 */
        Row(
            modifier = Modifier.padding(top = 8.dp)
        ) {
            Box(
                modifier = Modifier
                    .size(96.dp)
                    .clickable {
                        onImageClick()
                    }
            ) {
                ProductImage(
                    product.image,
                    Modifier.fillMaxSize()
                )

                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .align(Alignment.BottomEnd)
                        .padding(4.dp)
                        .size(24.dp)
                        .clip(CircleShape)
                        .background(Color.White)
                ) {
                    Icon(
                        Icons.Filled.Edit,
                        contentDescription = "사진 변경",
                        modifier = Modifier.size(14.dp)
                    )
                }
            }

            Column(
                modifier = Modifier
                    .weight(1f)
                    .padding(start = 16.dp)
            ) {
                Text(
                    product.name,
                    fontSize = 17.sp,
                    fontWeight = FontWeight.Medium,
                    modifier = Modifier.clickable {
                        onNameClick()
                    }
                )

                Text(
                    "%,d 원".format(product.price),
                    fontSize = 16.sp,
                    modifier = Modifier
                        .padding(top = 4.dp)
                        .clickable {
                            onPriceClick()
                        }
                )

                Spacer(modifier = Modifier.height(12.dp))

                /* 상품 수량 조절 */
                QtyStepper(
                    product.qty,
                    onQtyChange
                )
            }

            /* 상품 삭제 버튼 */
            IconButton(
                onClick = onDelete
            ) {
                Icon(
                    Icons.Filled.Close,
                    contentDescription = "삭제",
                    tint = Color.Gray
                )
            }
        }

        HorizontalDivider(
            modifier = Modifier.padding(top = 12.dp)
        )
    }
}

@Composable
private fun ProductImage(
    image: Bitmap?,
    modifier: Modifier
) {
    Box(
        contentAlignment = Alignment.Center,
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(Color(0xFFF2F2F2))
    ) {
        if (image != null) {
            Image(
                image.asImageBitmap(),
                contentDescription = null,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize()
            )
        } else {
            Icon(
                Icons.Filled.ShoppingCart,
                contentDescription = null,
                tint = Color.LightGray
            )
        }
    }
}

@Composable
private fun QtyStepper(
    qty: Int,
    onChange: (Int) -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically
    ) {
        OutlinedButton(
            onClick = {
                if (qty > 0) onChange(qty - 1)
            },
            shape = CircleShape,
            contentPadding = PaddingValues(0.dp),
            modifier = Modifier.size(30.dp)
        ) {
            Text("−", color = SnapGreen)
        }

        Text(
            "%02d".format(qty),
            fontSize = 16.sp,
            textAlign = TextAlign.Center,
            modifier = Modifier.width(44.dp)
        )

        OutlinedButton(
            onClick = {
                onChange(qty + 1)
            },
            shape = CircleShape,
            contentPadding = PaddingValues(0.dp),
            modifier = Modifier.size(30.dp)
        ) {
            Text("+", color = SnapGreen)
        }
    }
}

@Composable
private fun ProductAddDialog(
    usedShelves: List<Int>,
    onDismiss: () -> Unit,
    onAdd: (
        name: String,
        price: Int,
        qty: Int,
        shelf: Int?,
        image: Bitmap?
    ) -> Unit
) {
    val context = LocalContext.current

    /* 입력 상태 */
    var image by remember { mutableStateOf<Bitmap?>(null) }
    var slot by remember { mutableStateOf<Int?>(null) }
    var name by remember { mutableStateOf("") }
    var price by remember { mutableStateOf("") }
    var qty by remember { mutableIntStateOf(1) }
    var message by remember { mutableStateOf<String?>(null) }

    /* 사진 선택 */
    val photoPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.PickVisualMedia()
    ) { uri ->
        if (uri != null) {
            image = context.contentResolver
                .openInputStream(uri)
                ?.use { BitmapFactory.decodeStream(it) }
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
                .background(Color.White)
                .systemBarsPadding()
                .imePadding()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 24.dp)
        ) {
            Text(
                "상품 등록",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 16.dp, bottom = 24.dp)
            )

            /* 상품 사진 등록 */
            Box(
                contentAlignment = Alignment.Center,
                modifier = Modifier
                    .align(Alignment.CenterHorizontally)
                    .size(96.dp)
                    .clip(RoundedCornerShape(12.dp))
                    .border(
                        1.dp,
                        Color.LightGray,
                        RoundedCornerShape(12.dp)
                    )
                    .clickable {
                        photoPicker.launch(
                            PickVisualMediaRequest(
                                ActivityResultContracts.PickVisualMedia.ImageOnly
                            )
                        )
                    }
            ) {
                val picked = image

                if (picked != null) {
                    Image(
                        picked.asImageBitmap(),
                        contentDescription = null,
                        contentScale = ContentScale.Crop,
                        modifier = Modifier.fillMaxSize()
                    )
                } else {
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally
                    ) {
                        Icon(
                            Icons.Filled.Add,
                            contentDescription = "사진 등록",
                            tint = Color.Gray
                        )

                        Text(
                            "0/1",
                            fontSize = 13.sp,
                            color = Color.Gray
                        )
                    }
                }
            }

            /* 진열대 번호 선택 */
            Text(
                "진열대 번호",
                fontSize = 15.sp,
                fontWeight = FontWeight.Medium,
                modifier = Modifier.padding(top = 24.dp, bottom = 8.dp)
            )

            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                (1..SHELVES).forEach { n ->
                    val selected = slot == n

                    OutlinedButton(
                        onClick = {
                            slot = if (selected) null else n
                        },
                        enabled = n !in usedShelves,
                        colors = ButtonDefaults.outlinedButtonColors(
                            containerColor = if (selected) {
                                SnapGreen
                            } else {
                                Color.Transparent
                            },
                            contentColor = if (selected) {
                                Color.White
                            } else {
                                SnapGreen
                            }
                        ),
                        contentPadding = PaddingValues(0.dp),
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("${n}번")
                    }
                }
            }

            /* 상품명 입력 */
            Text(
                "상품명",
                fontSize = 15.sp,
                fontWeight = FontWeight.Medium,
                modifier = Modifier.padding(top = 20.dp, bottom = 8.dp)
            )

            OutlinedTextField(
                value = name,
                onValueChange = {
                    name = it
                },
                singleLine = true,
                shape = RoundedCornerShape(12.dp),
                modifier = Modifier.fillMaxWidth()
            )

            /* 가격 입력 */
            Text(
                "가격",
                fontSize = 15.sp,
                fontWeight = FontWeight.Medium,
                modifier = Modifier.padding(top = 20.dp, bottom = 8.dp)
            )

            OutlinedTextField(
                value = price,
                onValueChange = {
                    price = it
                },
                singleLine = true,
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Number
                ),
                shape = RoundedCornerShape(12.dp),
                modifier = Modifier.fillMaxWidth()
            )

            /* 수량 설정 */
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(top = 20.dp)
            ) {
                Text(
                    "수량",
                    fontSize = 15.sp,
                    fontWeight = FontWeight.Medium,
                    modifier = Modifier.weight(1f)
                )

                QtyStepper(qty) {
                    qty = it
                }
            }

            /* 취소, 등록 버튼 */
            Row(
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 32.dp)
            ) {
                OutlinedButton(
                    onClick = onDismiss,
                    modifier = Modifier
                        .weight(1f)
                        .height(44.dp)
                ) {
                    Text("취소", color = SnapGreen)
                }

                Button(
                    onClick = {
                        val p = price.toIntOrNull()

                        when {
                            name.isBlank() -> {
                                message = "상품명을 입력해 주세요."
                            }

                            p == null -> {
                                message = "가격을 숫자로 입력해 주세요."
                            }

                            else -> {
                                onAdd(name, p, qty, slot, image)
                            }
                        }
                    },
                    modifier = Modifier
                        .weight(1f)
                        .height(44.dp)
                ) {
                    Text("등록하기")
                }
            }
        }

        /* 입력 오류 메시지 */
        message?.let {
            AlertDialog(
                onDismissRequest = {
                    message = null
                },
                containerColor = Color.White,
                text = {
                    Text(
                        it,
                        textAlign = TextAlign.Center,
                        modifier = Modifier.fillMaxWidth()
                    )
                },
                confirmButton = {
                    Button(
                        onClick = {
                            message = null
                        }
                    ) {
                        Text("확인")
                    }
                }
            )
        }
    }
}